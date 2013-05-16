// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOGGER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOGGER_H_

#include <string>
#include <vector>

#include "chrome/browser/google_apis/event_logger.h"

namespace sync_file_system {
// Originally wanted to use 'logging' here, but it conflicts with
// base/logging.h, and breaks DCHECK() and friends.
namespace util {

// Logs a message using printf format.
// This function can be called from any thread.
void Log(const char* format, ...) PRINTF_FORMAT(1, 2);

// Returns the log history.
// This function can be called from any thread.
std::vector<google_apis::EventLogger::Event> GetLogHistory();

}  // namespace util
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOGGER_H_
