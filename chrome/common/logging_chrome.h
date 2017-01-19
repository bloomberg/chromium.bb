// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_LOGGING_CHROME_H_
#define CHROME_COMMON_LOGGING_CHROME_H_

#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace logging {

// Call to initialize logging for Chrome. This sets up the chrome-specific
// logfile naming scheme and might do other things like log modules and
// setting levels in the future.
//
// The main process might want to delete any old log files on startup by
// setting delete_old_log_file, but the renderer processes should not, or
// they will delete each others' logs.
//
// XXX
// Setting suppress_error_dialogs to true disables any dialogs that would
// normally appear for assertions and crashes, and makes any catchable
// errors (namely assertions) available via GetSilencedErrorCount()
// and GetSilencedError().
void InitChromeLogging(const base::CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file);

#if defined(OS_CHROMEOS)
// Get the log file directory path.
base::FilePath GetSessionLogDir(const base::CommandLine& command_line);

// Get the log file location.
base::FilePath GetSessionLogFile(const base::CommandLine& command_line);

// Redirects chrome logging to the appropriate session log dir.
void RedirectChromeLogging(const base::CommandLine& command_line);
#endif

// Call when done using logging for Chrome.
void CleanupChromeLogging();

// Returns the fully-qualified name of the log file.
base::FilePath GetLogFileName();

// Returns true when error/assertion dialogs are not to be shown, false
// otherwise.
bool DialogsAreSuppressed();

// Inserts timestamp before file extension in the format
// "_yymmdd-hhmmss".
base::FilePath GenerateTimestampedName(const base::FilePath& base_path,
                                       base::Time timestamp);
}  // namespace logging

#endif  // CHROME_COMMON_LOGGING_CHROME_H_
