// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pwd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <cups/backend.h>

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

#include "cloud_print/virtual_driver/posix/printer_driver_util_posix.h"

void WriteToTemp(FILE* input_pdf, FilePath output_path) {
  FILE* output_pdf;
  char buffer[128];
  output_pdf = fopen(output_path.value().c_str(), "w");
  if (output_pdf == NULL) {
    LOG(ERROR) << "Unable to open file handle for writing output file";
    exit(CUPS_BACKEND_CANCEL);
  }
  // Read from input file or stdin and write to output file.
  while (fgets(buffer, sizeof(buffer), input_pdf) != NULL) {
    fputs(buffer, output_pdf);
  }

  LOG(INFO) << "Successfully wrote output file";
  // ensure everything is written, then close the files.
  fflush(output_pdf);
  fclose(input_pdf);
  fclose(output_pdf);
}

void SetUser(const char* user) {
  struct passwd* calling_user = NULL;
  calling_user = getpwnam(user);
  if (calling_user == NULL) {
    LOG(ERROR) << "Unable to get calling user";
    exit(CUPS_BACKEND_CANCEL);
  }
  if (setgid(calling_user->pw_gid) == -1) {
    LOG(ERROR) << "Unable to set group ID";
    exit(CUPS_BACKEND_CANCEL);
  }
  if (!setuid(calling_user->pw_uid) == -1)  {
    LOG(ERROR) << "Unable to set UID";
    exit(CUPS_BACKEND_CANCEL);
  }

  LOG(INFO) << "Successfully set user and group ID";
}



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
    fprintf(stderr, "Usage: GCP-Driver job-id user"
                    "title copies options [file]\n");
    return 0;
  }

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

  // Get temp directory to hold spool file.
  if (!PathService::Get(base::DIR_TEMP, &temp_dir)) {
    LOG(ERROR) << "Unable to get DIR_TEMP";
    return CUPS_BACKEND_CANCEL;
  }
  // Get details from command line and set spool path.
  current_user = argv[2];
  job_title = argv[3];
  job_id = argv[1];
  file_name = current_user + "-"  + job_title + "-" + job_id;
  FilePath output_path = temp_dir.Append(file_name);

  if (argc == 7) {
    // Read from file if specified.
    FILE* input_pdf = fopen(argv[6], "r");
    if (input_pdf == NULL) {
      LOG(ERROR) << "Unable to read input PDF";
      return CUPS_BACKEND_CANCEL;
    }
    // File is opened.
    WriteToTemp(input_pdf, output_path);
  } else {
    // Otherwise, read from stdin.
    WriteToTemp(stdin, output_path);
  }

  // Set File permissions to allow non-sudo user to read spool file.
  if (chmod(output_path.value().c_str(),
            S_IRUSR|S_IWUSR|S_IROTH|S_IWOTH) != 0) {
    LOG(ERROR) << "Unable to change file permissions";
    return CUPS_BACKEND_CANCEL;
  }

  pid_t pid = fork();

  if (!pid) {
    // In child process.

    // Set the user to the one that initiated print job.
    SetUser(argv[2]);

    // Launch Chrome and pass the print job onto Cloud Print.
    LaunchPrintDialog(output_path.value(), job_title, current_user);

    return 0;
  }
  // back in parent process.
  // wait for child, then terminate.
  waitpid(pid, NULL, 0);
  return 0;
}
