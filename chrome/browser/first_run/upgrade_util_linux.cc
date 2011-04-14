// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util.h"

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/process_util.h"
#include "chrome/browser/first_run/upgrade_util_linux.h"

namespace {

double saved_last_modified_time_of_exe = 0;

}  // namespace

namespace upgrade_util {

bool RelaunchChromeBrowser(const CommandLine& command_line) {
  return base::LaunchApp(command_line, false, false, NULL);
}

bool IsUpdatePendingRestart() {
  return saved_last_modified_time_of_exe != GetLastModifiedTimeOfExe();
}

void SaveLastModifiedTimeOfExe() {
  saved_last_modified_time_of_exe = GetLastModifiedTimeOfExe();
}

double GetLastModifiedTimeOfExe() {
  FilePath exe_file_path;
  if (!PathService::Get(base::FILE_EXE, &exe_file_path)) {
    LOG(WARNING) << "Failed to get FilePath object for FILE_EXE.";
    return saved_last_modified_time_of_exe;
  }
  base::PlatformFileInfo exe_file_info;
  if (!file_util::GetFileInfo(exe_file_path, &exe_file_info)) {
    LOG(WARNING) << "Failed to get FileInfo object for FILE_EXE - "
                 << exe_file_path.value();
    return saved_last_modified_time_of_exe;
  }
  return exe_file_info.last_modified.ToDoubleT();
}

}  // namespace upgrade_util
