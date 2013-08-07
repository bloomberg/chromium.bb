// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_LOGGING_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_LOGGING_H_

#include <vector>

#include "chrome/browser/drive/event_logger.h"

namespace drive {
// Originally wanted to use 'logging' here, but it conflicts with
// base/logging.h, and breaks DCHECK() and friends.
namespace util {

// Logs a message using printf format.
// This function can be called from any thread.
void Log(
    logging::LogSeverity severity, const char* format, ...) PRINTF_FORMAT(2, 3);

// Returns the log history.
// This function can be called from any thread.
std::vector<EventLogger::Event> GetLogHistory();

}  // namespace util
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_LOGGING_H_
