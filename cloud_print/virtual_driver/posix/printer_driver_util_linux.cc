// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/backend.h>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"

#include "cloud_print/virtual_driver/posix/printer_driver_util_posix.h"
#include "cloud_print/virtual_driver/virtual_driver_switches.h"

namespace printer_driver_util {

void LaunchPrintDialog(const std::string& output_path,
                       const std::string& job_title,
                       const std::string& current_user,
                       const std::string& print_ticket) {
  std::string set_var;

  // Set Environment variable to control display.
  set_var = "/home/" + current_user + "/.Xauthority";
  if (setenv("DISPLAY", ":0.0", 0) == -1) {
    LOG(ERROR) << "Unable to set DISPLAY environment variable";
  }
  if (setenv("XAUTHORITY", set_var.c_str(), 0) == -1) {
    LOG(ERROR) << "Unable to set XAUTHORITY environment variable";
  }

  // Construct the call to Chrome
  FilePath chrome_path("google-chrome");
  FilePath job_path(output_path);
  CommandLine command_line(chrome_path);
  command_line.AppendSwitchPath(switches::kCloudPrintFile, job_path);
  command_line.AppendSwitchNative(switches::kCloudPrintJobTitle, job_title);
  command_line.AppendSwitch(switches::kCloudPrintDeleteFile);
  command_line.AppendSwitchNative(switches::kCloudPrintPrintTicket,
                                  print_ticket);
  LOG(INFO) << "Call to chrome is " << command_line.GetCommandLineString();
  if (system(command_line.GetCommandLineString().c_str()) == -1) {
    LOG(ERROR) << "Unable to call Chrome";
    exit(CUPS_BACKEND_CANCEL);
  }
  LOG(INFO) << "Call to Chrome succeeded";
}

}  // namespace printer_driver_util

