// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_logging.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"

namespace {

// Path to the WebDriver log file.
const FilePath::CharType kLogPath[] = FILE_PATH_LITERAL("chromedriver.log");

}  // namespace

namespace webdriver {

void InitWebDriverLogging(int min_log_level) {
  bool success = InitLogging(
      kLogPath,
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  if (!success) {
    PLOG(ERROR) << "Unable to initialize logging";
  }
  logging::SetLogItems(false,  // enable_process_id
                       false,  // enable_thread_id
                       true,   // enable_timestamp
                       false); // enable_tickcount
  logging::SetMinLogLevel(min_log_level);
}

bool GetLogContents(std::string* log_contents) {
  return file_util::ReadFileToString(FilePath(kLogPath), log_contents);
}

}  // namespace webdriver
