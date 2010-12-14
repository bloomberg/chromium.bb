// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/installer/util/logging_installer.h"

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/installer/util/master_preferences.h"
#include "chrome/installer/util/master_preferences_constants.h"
#include "chrome/installer/util/util_constants.h"

// {93BCE0BF-3FAF-43b1-9E28-BEB6FAB5ECE7}
static const GUID kSetupTraceProvider = { 0x93bce0bf, 0x3faf, 0x43b1,
    { 0x9e, 0x28, 0xbe, 0xb6, 0xfa, 0xb5, 0xec, 0xe7 } };

namespace installer {

// This should be true for the period between the end of
// InitInstallerLogging() and the beginning of EndInstallerLogging().
bool installer_logging_ = false;

void InitInstallerLogging(const installer_util::MasterPreferences& prefs) {
  if (installer_logging_)
    return;

  bool value = false;
  if (prefs.GetBool(installer_util::master_preferences::kDisableLogging,
                    &value) && value) {
    installer_logging_ = true;
    return;
  }

  logging::InitLogging(GetLogFilePath(prefs).value().c_str(),
                       logging::LOG_ONLY_TO_FILE,
                       logging::LOCK_LOG_FILE,
                       logging::DELETE_OLD_LOG_FILE);

  if (prefs.GetBool(installer_util::master_preferences::kVerboseLogging,
                    &value) && value) {
    logging::SetMinLogLevel(logging::LOG_VERBOSE);
  } else {
    logging::SetMinLogLevel(logging::LOG_ERROR);
  }

  // Enable ETW logging.
  logging::LogEventProvider::Initialize(kSetupTraceProvider);

  installer_logging_ = true;
}

void EndInstallerLogging() {
  logging::CloseLogFile();

  installer_logging_ = false;
}

FilePath GetLogFilePath(const installer_util::MasterPreferences& prefs) {
  std::string path;
  prefs.GetString(installer_util::master_preferences::kLogFile, &path);
  if (!path.empty()) {
    return FilePath(UTF8ToWide(path));
  }

  std::wstring log_filename = prefs.install_chrome_frame() ?
      L"chrome_frame_installer.log" : L"chrome_installer.log";

  FilePath log_path;
  if (PathService::Get(base::DIR_TEMP, &log_path)) {
    log_path = log_path.Append(log_filename);
    return log_path;
  } else {
    return FilePath(log_filename);
  }
}

}  // namespace installer
