/*
    Copyright (C) 2014 Luis M. Santos
    Contact: luismigue1234@hotmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with This program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <iostream>
#include <fstream>
#include "html2faxcover.h"
#include "data_base.h"


int main(int argc, char* argv[])
{
    /* Init
    */
    h2fax::faxrecvd_args args;
    h2fax::fax fax_info;
    /*Reroute output streams*/
    //Let's redirect cerr
    std::ofstream cerrLog("/tmp/cerrlog");
    std::streambuf *cerr_copy = std::cerr.rdbuf();
    if(cerrLog.is_open())
       std::cerr.rdbuf(cerrLog.rdbuf());
    //Let's redirect cout
    std::ofstream coutLog("/tmp/coutlog");
    std::streambuf *cout_copy = std::cout.rdbuf();
    if(cerrLog.is_open())
       std::cout.rdbuf(cerrLog.rdbuf());
    /*Create other variables needed*/
    data_base settings("/etc/html2faxcover/settings");
    std::string tiff2pdf_path = settings.GetStrFromData("tiff2pdf");
    std::string fax_path = settings.GetStrFromData("fax_path");
    std::string tiffcp = settings.GetStrFromData("tiffcp");
    std::string fileinfo_prog = settings.GetStrFromData("fileinfo");
    std::string sendmail = settings.GetStrFromData("sendmail");
    std::string printer_cmd = settings.GetStrFromData("print_cmd");
    std::string printer_name = settings.GetStrFromData("printer");
    std::string printer_flag = settings.GetStrFromData("print_flag");
    std::string recipient = "";
    bool enable_fax_annotation = settings.GetIntFromData("fax_annotation");
    /*Obtain database info*/
    //Let's store the database connection info
    args.Database = settings.GetStrFromData("database").c_str();
    args.Host = settings.GetStrFromData("host").c_str();
    args.Username = settings.GetStrFromData("username").c_str();
    args.Password = settings.GetStrFromData("password").c_str();
    //Let's save the c_string pointers in the regular spot for arguments
    args.database = args.Database.c_str();
    args.host = args.Host.c_str();
    args.username = args.Username.c_str();
    args.password = args.Password.c_str();
    /*Build a database connection*/
    MySQL db(args.database, args.username, args.password, args.host);

    /*Prog
    */
    std::string tiff_name = "fax.tiff";
    std::string pdf_name = "fax.pdf";
    std::string thumbnail = "thumb.gif";

    h2fax::logMsg("Obtain Fax Info!");
    fax_info = h2fax::faxinfo(fileinfo_prog, args.tiff_file);

    h2fax::logMsg("Copying tiff file using tiffcp!");
    std::string fax_file = fax_path + PATH_SLASH + fax_info.docTime.Day + fax_info.CallID1 + PATH_SLASH + tiff_name;
    h2fax::exec_cmd(args.tiff_file, tiffcp, fax_file);

    h2fax::logMsg("Creating PDF version...");
    std::string pdf_file = fax_path + PATH_SLASH + fax_info.docTime.Day + fax_info.CallID1 + PATH_SLASH + pdf_name;
    h2fax::exec_cmd(fax_file.c_str(), tiff2pdf_path, pdf_file);

    //Check fax number in addressbook!
    h2fax::logMsg("Checking caller ID...");
    std::string faxnumid = "";
    db.queryDB(db.prepareStatement("AddressBookFax", "abook_id", std::string("faxnumber = ") + "'" + fax_info.CallID1 + "'","","",SELECT | FROM | WHERE));
    if(db.hasResults())//Find out if the fax number is registered in the address book
    {
        db.getResult(faxnumid);
        db.queryDB(db.prepareStatement("AddressBook", "Company", std::string("abook_id = ") + faxnumid,"","",SELECT|FROM|WHERE));
        db.getResult(fax_info.CallID2);
    }
    else//Create a new entry for this fax with the information available
    {
        //Create entry in table AddressBook
        db.queryDB(db.prepareStatement("AddressBook", "Company", fax_info.CallID2,"","",INSERT|INTO));
        //Select the autogenerated id for CallID2 in AddressBook after insertion
        db.queryDB(db.prepareStatement("AddressBook", "abook_id", std::string("Company = ") + "'" + fax_info.CallID2 + "'","","",SELECT|FROM|WHERE));
        db.getResult(faxnumid);
        db.queryDB(db.prepareStatement("AddressBookFax", std::string("abook_id,faxnumber,email,description,to_person,to_location,") +
                                    "to_voicenumber,faxcatid,faxfrom,faxto,printer",
                                    faxnumid + SQL_COMA + fax_info.CallID1 + SQL_COMA + SQL_NULL + SQL_COMA + SQL_NULL + SQL_COMA
                                     + SQL_NULL + SQL_COMA + SQL_NULL + SQL_COMA + SQL_NULL + SQL_COMA
                                     + SQL_COMA + SQL_NULL + SQL_COMA + SQL_ZERO + SQL_COMA + SQL_ZERO + SQL_COMA + SQL_NULL + SQL_COMA,""
                                     ,"",INSERT|INTO));
    }

    //Let's enable DID routing!
    h2fax::logMsg("Enabling DID routing...");
    std::string didnum_id = "";
    db.queryDB(db.prepareStatement("didroute", "didr_id", std::string("routecode = ") + "'" + fax_info.CallID3 + "'","","",SELECT | FROM | WHERE));
    if(db.hasResults())//Find out if the DID number is registered in the database
    {
        db.getResult(didnum_id);
    }
    else//Create a new entry for this fax with the information available
    {
        //Create entry in table didroute
        db.queryDB(db.prepareStatement("didroute", "routecode,alias,contact,printer,faxcatid",
                                    fax_info.CallID3 + SQL_COMA + SQL_NULL + SQL_COMA + SQL_NULL + SQL_COMA
                                     + SQL_NULL + SQL_COMA + SQL_NULL,"","",INSERT|INTO));
    }

    //Let's get the fax category id before we move on!
    h2fax::logMsg("Getting fax category id...");
    std::string faxcatid = "0";
    //Let's get the category id
    db.queryDB((db.prepareStatement("modems", "faxcatid", std::string("device = ") + args.modemdev,"","",SELECT|FROM|WHERE)));
    if(db.hasResults())
    {
        db.getResult(faxcatid);
    }

    //Add Fax to inbox!
    h2fax::logMsg("Adding Fax to Inbox...");
    std::string faxid = "";
    //Write fax entry to the database (table faxarchive)
    db.queryDB(db.prepareStatement("faxarchive", "faxpath,pages,didr_id,archstamp,faxnumid,origfaxnum,faxcatid",
                                   fax_path + SQL_COMA + fax_info.Pages + SQL_COMA + didnum_id + SQL_COMA +
                                   fax_info.docTime.Day + " " + fax_info.docTime.Hour + SQL_COMA + faxnumid
                                   + SQL_COMA + fax_info.Sender + SQL_COMA + faxcatid,"","",INSERT|INTO));
    db.queryDB(db.prepareStatement("didroute", "didr_id", std::string("routecode = ") + "'" + fax_info.CallID3 + "'","","",SELECT | FROM | WHERE));
    if(db.hasResults())//Find out if we successfully added the fax entry
    {
        db.getResult(didnum_id);
        h2fax::logMsg("Successfully added fax entry to database! Houston, we have a lift off! *Applauses*");
    }
    else//log an error
    {
        h2fax::logMsg(std::string("Oops, the ship sunk! This is why we can't have nice things... The program was not able") +
                      "to add the fax entry to the database! :(");
    }

    //Fax annotation
    h2fax::logMsg("Processing fax annotation!");
    if(enable_fax_annotation)
    {
        //Future versions should attempt to have a block of code here like in AvantFax's faxrecvd!
    }

    //Let's send an email!
    h2fax::logMsg("Preparing to send e-mail!");
    //Let's compile a list of destinations to which we want to send the email!
    db.queryDB(db.prepareStatement("UserAccount", "email", std::string("modemdevs = ") + args.modemdev,"","",SELECT|FROM|WHERE));
    if(db.hasResults())
    {
        std::string tmp;
        recipient += "\"";
        for(size_t i = 0; i < db.rowCount(); i++)
        {
            db.getResult(tmp, i);
            recipient += tmp;
            recipient += ",";
        }
        recipient += "\"";
    }
    //Send the fax to all users associated with the modem
    h2fax::exec_cmd(std::string("\"/a " + fax_file + " /a " + pdf_file + "\"").c_str(), "sendmail", recipient + "\"Fax_From: " + fax_info.CallID2 + "\"");

    //Print Support
    h2fax::logMsg("Printing Fax to preferred printer...");
    if(printer_name == "")//If empty, print in default printer!
    {
        h2fax::exec_cmd(pdf_file.c_str(), printer_cmd, "");
    }
    else//Otherwise, print to the printer indicated in the settings file!
    {
        h2fax::exec_cmd("", printer_cmd, "-" + printer_flag + " " + printer_name + " " + pdf_file);
    }
}
