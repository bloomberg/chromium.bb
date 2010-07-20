// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

// Need to include this before most other files because it defines
// IPC_MESSAGE_LOG_ENABLED. We need to use it to define
// IPC_MESSAGE_MACROS_LOG_ENABLED so render_messages.h will generate the
// ViewMsgLog et al. functions.
#include "ipc/ipc_message.h"

// On Windows, the about:ipc dialog shows IPCs; on POSIX, we hook up a
// logger in this file.  (We implement about:ipc on Mac but implement
// the loggers here anyway).  We need to do this real early to be sure
// IPC_MESSAGE_MACROS_LOG_ENABLED doesn't get undefined.
#if defined(OS_POSIX) && defined(IPC_MESSAGE_LOG_ENABLED)
#define IPC_MESSAGE_MACROS_LOG_ENABLED
#include "chrome/common/devtools_messages.h"
#include "chrome/common/plugin_messages.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/worker_messages.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

#include <fstream>

#include "chrome/common/logging_chrome.h"

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug_util.h"
#include "base/env_var.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "ipc/ipc_logging.h"
#if defined(OS_WIN)
#include "base/logging_win.h"
#include <initguid.h>
#endif

// When true, this means that error dialogs should not be shown.
static bool dialogs_are_suppressed_ = false;

// This should be true for exactly the period between the end of
// InitChromeLogging() and the beginning of CleanupChromeLogging().
static bool chrome_logging_initialized_ = false;

#if defined(OS_WIN)
// {7FE69228-633E-4f06-80C1-527FEA23E3A7}
DEFINE_GUID(kChromeTraceProviderName,
  0x7fe69228, 0x633e, 0x4f06, 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7);
#endif

// Assertion handler for logging errors that occur when dialogs are
// silenced.  To record a new error, pass the log string associated
// with that error in the str parameter.
MSVC_DISABLE_OPTIMIZE();
static void SilentRuntimeAssertHandler(const std::string& str) {
  DebugUtil::BreakDebugger();
}
static void SilentRuntimeReportHandler(const std::string& str) {
}
MSVC_ENABLE_OPTIMIZE();

// Suppresses error/assertion dialogs and enables the logging of
// those errors into silenced_errors_.
static void SuppressDialogs() {
  if (dialogs_are_suppressed_)
    return;

  logging::SetLogAssertHandler(SilentRuntimeAssertHandler);
  logging::SetLogReportHandler(SilentRuntimeReportHandler);

#if defined(OS_WIN)
  UINT new_flags = SEM_FAILCRITICALERRORS |
                   SEM_NOGPFAULTERRORBOX |
                   SEM_NOOPENFILEERRORBOX;

  // Preserve existing error mode, as discussed at http://t/dmea
  UINT existing_flags = SetErrorMode(new_flags);
  SetErrorMode(existing_flags | new_flags);
#endif

  dialogs_are_suppressed_ = true;
}

namespace logging {

LoggingDestination DetermineLogMode(const CommandLine& command_line) {
  // only use OutputDebugString in debug mode
#ifdef NDEBUG
  bool enable_logging = false;
  const char *kInvertLoggingSwitch = switches::kEnableLogging;
  const logging::LoggingDestination kDefaultLoggingMode =
      logging::LOG_ONLY_TO_FILE;
#else
  bool enable_logging = true;
  const char *kInvertLoggingSwitch = switches::kDisableLogging;
  const logging::LoggingDestination kDefaultLoggingMode =
      logging::LOG_TO_BOTH_FILE_AND_SYSTEM_DEBUG_LOG;
#endif

  if (command_line.HasSwitch(kInvertLoggingSwitch))
    enable_logging = !enable_logging;

  logging::LoggingDestination log_mode;
  if (enable_logging) {
    // Let --enable-logging=stderr force only stderr, particularly useful for
    // non-debug builds where otherwise you can't get logs to stderr at all.
    if (command_line.GetSwitchValueASCII(switches::kEnableLogging) == "stderr")
      log_mode = logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG;
    else
      log_mode = kDefaultLoggingMode;
  } else {
    log_mode = logging::LOG_NONE;
  }
  return log_mode;
}

#if defined(OS_CHROMEOS)
void SetUpSymlink(const FilePath& symlink_path, const FilePath& new_log_path) {
  // We don't care if the unlink fails; we're going to continue anyway.
  if (unlink(symlink_path.value().c_str()) == -1)
    PLOG(WARNING) << "Unable to unlink " << symlink_path.value();
  if (symlink(new_log_path.value().c_str(),
              symlink_path.value().c_str()) == -1) {
    PLOG(ERROR) << "Unable to create symlink " << symlink_path.value()
                << " pointing at " << new_log_path.value();
  }
}

FilePath TimestampLog(const FilePath& new_log_file, base::Time timestamp) {
  base::Time::Exploded time_deets;
  timestamp.LocalExplode(&time_deets);
  std::string suffix = StringPrintf("_%02d%02d%02d-%02d%02d%02d",
                                    time_deets.year,
                                    time_deets.month,
                                    time_deets.day_of_month,
                                    time_deets.hour,
                                    time_deets.minute,
                                    time_deets.second);
  FilePath new_log_path = new_log_file.InsertBeforeExtension(suffix);
  SetUpSymlink(new_log_file, new_log_path);

  return new_log_path;
}

void RedirectChromeLogging(const FilePath& new_log_dir,
                           const CommandLine& command_line,
                           OldFileDeletionState delete_old_log_file) {
  FilePath log_file_name = GetLogFileName().BaseName();
  FilePath new_log_path =
      TimestampLog(new_log_dir.Append(log_file_name), base::Time::Now());
  InitLogging(new_log_path.value().c_str(),
              DetermineLogMode(command_line),
              logging::LOCK_LOG_FILE,
              delete_old_log_file);
}
#endif

void InitChromeLogging(const CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file) {
  DCHECK(!chrome_logging_initialized_) <<
    "Attempted to initialize logging when it was already initialized.";

#if defined(OS_POSIX) && defined(IPC_MESSAGE_LOG_ENABLED)
  IPC::Logging::SetLoggerFunctions(g_log_function_mapping);
#endif

  FilePath log_path = GetLogFileName();
#if defined(OS_CHROMEOS)
  log_path = TimestampLog(log_path, base::Time::Now());
#endif

  logging::InitLogging(log_path.value().c_str(),
                       DetermineLogMode(command_line),
                       logging::LOCK_LOG_FILE,
                       delete_old_log_file);

  // we want process and thread IDs because we have a lot of things running
  logging::SetLogItems(true, true, false, true);

  // We call running in unattended mode "headless", and allow
  // headless mode to be configured either by the Environment
  // Variable or by the Command Line Switch.  This is for
  // automated test purposes.
  scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());
  if (env->HasEnv(env_vars::kHeadless) ||
      command_line.HasSwitch(switches::kNoErrorDialogs))
    SuppressDialogs();

  std::string log_filter_prefix =
      command_line.GetSwitchValueASCII(switches::kLogFilterPrefix);
  logging::SetLogFilterPrefix(log_filter_prefix.c_str());

  // Use a minimum log level if the command line has one, otherwise set the
  // default to LOG_WARNING.
  std::string log_level = command_line.GetSwitchValueASCII(
      switches::kLoggingLevel);
  int level = 0;
  if (StringToInt(log_level, &level)) {
    if ((level >= 0) && (level < LOG_NUM_SEVERITIES))
      logging::SetMinLogLevel(level);
  } else {
    logging::SetMinLogLevel(LOG_WARNING);
  }

#if defined(OS_WIN)
  // Enable trace control and transport through event tracing for Windows.
  if (env->HasEnv(env_vars::kEtwLogging))
    logging::LogEventProvider::Initialize(kChromeTraceProviderName);
#endif

  chrome_logging_initialized_ = true;
}

// This is a no-op, but we'll keep it around in case
// we need to do more cleanup in the future.
void CleanupChromeLogging() {
  DCHECK(chrome_logging_initialized_) <<
    "Attempted to clean up logging when it wasn't initialized.";

  CloseLogFile();

  chrome_logging_initialized_ = false;
}

FilePath GetLogFileName() {
  std::string filename;
  scoped_ptr<base::EnvVarGetter> env(base::EnvVarGetter::Create());
  if (env->GetEnv(env_vars::kLogFileName, &filename) && !filename.empty()) {
#if defined(OS_WIN)
    return FilePath(UTF8ToWide(filename).c_str());
#elif defined(OS_POSIX)
    return FilePath(filename.c_str());
#endif
  }

  const FilePath log_filename(FILE_PATH_LITERAL("chrome_debug.log"));
  FilePath log_path;

  if (PathService::Get(chrome::DIR_LOGS, &log_path)) {
    log_path = log_path.Append(log_filename);
    return log_path;
  } else {
    // error with path service, just use some default file somewhere
    return log_filename;
  }
}

bool DialogsAreSuppressed() {
  return dialogs_are_suppressed_;
}

size_t GetFatalAssertions(AssertionList* assertions) {
  // In this function, we don't assume that assertions is non-null, so
  // that if you just want an assertion count, you can pass in NULL.
  if (assertions)
    assertions->clear();
  size_t assertion_count = 0;

  std::ifstream log_file;
  log_file.open(GetLogFileName().value().c_str());
  if (!log_file.is_open())
    return 0;

  std::string utf8_line;
  std::wstring wide_line;
  while (!log_file.eof()) {
    getline(log_file, utf8_line);
    if (utf8_line.find(":FATAL:") != std::string::npos) {
      wide_line = UTF8ToWide(utf8_line);
      if (assertions)
        assertions->push_back(wide_line);
      ++assertion_count;
    }
  }
  log_file.close();

  return assertion_count;
}

}  // namespace logging
