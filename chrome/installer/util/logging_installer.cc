// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "chrome/installer/util/logging_installer.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/scoped_handle.h"
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

TruncateResult TruncateLogFileIfNeeded(const base::FilePath& log_file) {
  TruncateResult result = LOGFILE_UNTOUCHED;

  int64 log_size = 0;
  if (file_util::GetFileSize(log_file, &log_size) &&
      log_size > kMaxInstallerLogFileSize) {
    // Cause the old log file to be deleted when we are done with it.
    const int file_flags = base::PLATFORM_FILE_OPEN |
                           base::PLATFORM_FILE_READ |
                           base::PLATFORM_FILE_SHARE_DELETE |
                           base::PLATFORM_FILE_DELETE_ON_CLOSE;
    base::win::ScopedHandle old_log_file(
        base::CreatePlatformFile(log_file, file_flags, NULL, NULL));

    if (old_log_file.IsValid()) {
      result = LOGFILE_DELETED;
      base::FilePath tmp_log(log_file.value() + FILE_PATH_LITERAL(".tmp"));
      // Note that file_util::Move will attempt to replace existing files.
      if (file_util::Move(log_file, tmp_log)) {
        int64 offset = log_size - kTruncatedInstallerLogFileSize;
        std::string old_log_data(kTruncatedInstallerLogFileSize, 0);
        int bytes_read = base::ReadPlatformFile(old_log_file,
                                                offset,
                                                &old_log_data[0],
                                                kTruncatedInstallerLogFileSize);
        if (bytes_read > 0 &&
            (bytes_read == file_util::WriteFile(log_file,
                                                &old_log_data[0],
                                                bytes_read) ||
             file_util::PathExists(log_file))) {
          result = LOGFILE_TRUNCATED;
        }
      }
    } else if (file_util::Delete(log_file, false)) {
      // Couldn't get sufficient access to the log file, optimistically try to
      // delete it.
      result = LOGFILE_DELETED;
    }
  }

  return result;
}


void InitInstallerLogging(const installer::MasterPreferences& prefs) {
  if (installer_logging_)
    return;

  installer_logging_ = true;

  bool value = false;
  if (prefs.GetBool(installer::master_preferences::kDisableLogging,
                    &value) && value) {
    return;
  }

  base::FilePath log_file_path(GetLogFilePath(prefs));
  TruncateLogFileIfNeeded(log_file_path);

  logging::InitLogging(
      log_file_path.value().c_str(),
      logging::LOG_ONLY_TO_FILE,
      logging::LOCK_LOG_FILE,
      logging::APPEND_TO_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  if (prefs.GetBool(installer::master_preferences::kVerboseLogging,
                    &value) && value) {
    logging::SetMinLogLevel(logging::LOG_VERBOSE);
  } else {
    logging::SetMinLogLevel(logging::LOG_ERROR);
  }

  // Enable ETW logging.
  logging::LogEventProvider::Initialize(kSetupTraceProvider);
}

void EndInstallerLogging() {
  logging::CloseLogFile();

  installer_logging_ = false;
}

base::FilePath GetLogFilePath(const installer::MasterPreferences& prefs) {
  std::string path;
  prefs.GetString(installer::master_preferences::kLogFile, &path);
  if (!path.empty()) {
    return base::FilePath(UTF8ToWide(path));
  }

  std::wstring log_filename = prefs.install_chrome_frame() ?
      L"chrome_frame_installer.log" : L"chrome_installer.log";

  base::FilePath log_path;
  if (PathService::Get(base::DIR_TEMP, &log_path)) {
    log_path = log_path.Append(log_filename);
    return log_path;
  } else {
    return base::FilePath(log_filename);
  }
}

}  // namespace installer
