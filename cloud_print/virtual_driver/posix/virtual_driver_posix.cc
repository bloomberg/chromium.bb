// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/backend.h>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

#include "cloud_print/virtual_driver/posix/printer_driver_util_posix.h"

// Main function for backend.
int main(int argc, const char* argv[]) {
  // With no arguments, send identification string as required by CUPS.
  if (argc == 1) {
    printf("file GCP-driver:/ \"GCP Virtual Driver\" \"GCP Virtual Driver\" "
         "\"MFG:Google Inc.;MDL:GCP Virtual Driver;DES:GCP Virtual Driver;"
         "CLS:PRINTER;CMD:POSTSCRIPT;\"\n");
    return 0;
  }

  if (argc < 6 || argc > 7) {
    fprintf(stderr, "Usage: GCP-Driver job-id user "
        "title copies options [file]\n");
    return 0;
  }

  // We can run the backend as root or unpriveliged user lp
  // Since we want to launch the printer dialog as the user
  // that initiated the print job, we run the backend as root
  // and then setuid to the user that started the print job
  printer_driver_util::SetUser(argv[2]);

  // AtExitManager is necessary to use the path service.
  base::AtExitManager aemanager;
  // CommandLine is only setup to enable logging, never used subseqeuently.
  CommandLine::Init(argc, argv);
  // Location for the log file.
  std::string log_location = "/var/log/GCP-Driver.log";

  // Set up logging.
  logging::InitLogging(log_location.c_str(),
             logging::LOG_ONLY_TO_FILE,
             logging::LOCK_LOG_FILE,
             logging::APPEND_TO_OLD_LOG_FILE,
             logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  // Temporary directory to hold the output file.
  FilePath temp_dir;
  // Strings to hold details of current job.
  std::string current_user;
  std::string set_var;
  std::string job_title;
  std::string job_id;
  std::string file_name;
  std::string print_ticket;

  // Get temp directory to hold spool file.
  if (!PathService::Get(base::DIR_TEMP, &temp_dir)) {
    LOG(ERROR) << "Unable to get DIR_TEMP";
    return CUPS_BACKEND_CANCEL;
  }
  // Get details from command line and set spool path.
  current_user = argv[2];
  job_title = argv[3];
  job_id = argv[1];
  printer_driver_util::GetOptions(argv[5], &print_ticket);
  file_name = current_user + "-"  + job_title + "-" + job_id;
  FilePath output_path = temp_dir.Append(file_name);

  // However, the input file can only be read as root.
  if (!setuid(0))  {
    PLOG(ERROR) << "Unable to setuid back to 0";
  }

  if (argc == 7) {
    // Read from file if specified.
    FILE* input_pdf = fopen(argv[6], "r");
    if (input_pdf == NULL) {
      LOG(ERROR) << "Unable to read input PDF";
      return CUPS_BACKEND_CANCEL;
    }
    // File is opened.
    printer_driver_util::WriteToTemp(input_pdf, output_path);
  } else {
    // Otherwise, read from stdin.
    printer_driver_util::WriteToTemp(stdin, output_path);
  }

  // Change back to user to launch Chrome.
  printer_driver_util::SetUser(argv[2]);
  PLOG(ERROR) << print_ticket;
  // Launch Chrome and pass the print job onto Cloud Print.
  printer_driver_util::LaunchPrintDialog(output_path.value(), job_title,
                                         current_user, print_ticket);

  return 0;
}
