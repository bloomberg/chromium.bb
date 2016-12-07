// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "content/public/common/content_ipc_logging.h"
#define IPC_LOG_TABLE_ADD_ENTRY(msg_id, logger) \
    content::RegisterIPCLogger(msg_id, logger)
#include "chrome/common/all_messages.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "chrome/common/logging_chrome.h"

#include <fstream>  // NOLINT
#include <memory>  // NOLINT
#include <string>  // NOLINT

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/debugger.h"
#include "base/debug/dump_without_crashing.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/statistics_recorder.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/env_vars.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_logging.h"

#if defined(OS_CHROMEOS)
#include "chromeos/chromeos_switches.h"
#endif

#if defined(OS_WIN)
#include <initguid.h>
#include "base/logging_win.h"
#include "base/syslog_logging.h"
#include "chrome/install_static/install_details.h"
#endif

namespace {

// When true, this means that error dialogs should not be shown.
bool dialogs_are_suppressed_ = false;

// This should be true for exactly the period between the end of
// InitChromeLogging() and the beginning of CleanupChromeLogging().
bool chrome_logging_initialized_ = false;

// Set if we called InitChromeLogging() but failed to initialize.
bool chrome_logging_failed_ = false;

// This should be true for exactly the period between the end of
// InitChromeLogging() and the beginning of CleanupChromeLogging().
bool chrome_logging_redirected_ = false;

#if defined(OS_WIN)
// {7FE69228-633E-4f06-80C1-527FEA23E3A7}
const GUID kChromeTraceProviderName = {
    0x7fe69228, 0x633e, 0x4f06,
        { 0x80, 0xc1, 0x52, 0x7f, 0xea, 0x23, 0xe3, 0xa7 } };
#endif

// Assertion handler for logging errors that occur when dialogs are
// silenced.  To record a new error, pass the log string associated
// with that error in the str parameter.
MSVC_DISABLE_OPTIMIZE();
void SilentRuntimeAssertHandler(const std::string& str) {
  base::debug::BreakDebugger();
}
MSVC_ENABLE_OPTIMIZE();

// Suppresses error/assertion dialogs and enables the logging of
// those errors into silenced_errors_.
void SuppressDialogs() {
  if (dialogs_are_suppressed_)
    return;

  logging::SetLogAssertHandler(SilentRuntimeAssertHandler);

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

}  // anonymous namespace

namespace logging {

LoggingDestination DetermineLogMode(const base::CommandLine& command_line) {
  // only use OutputDebugString in debug mode
#ifdef NDEBUG
  bool enable_logging = false;
  const char *kInvertLoggingSwitch = switches::kEnableLogging;
  const logging::LoggingDestination kDefaultLoggingMode = logging::LOG_TO_FILE;
#else
  bool enable_logging = true;
  const char *kInvertLoggingSwitch = switches::kDisableLogging;
  const logging::LoggingDestination kDefaultLoggingMode = logging::LOG_TO_ALL;
#endif

  if (command_line.HasSwitch(kInvertLoggingSwitch))
    enable_logging = !enable_logging;

  logging::LoggingDestination log_mode;
  if (enable_logging) {
    // Let --enable-logging=stderr force only stderr, particularly useful for
    // non-debug builds where otherwise you can't get logs to stderr at all.
    if (command_line.GetSwitchValueASCII(switches::kEnableLogging) == "stderr")
      log_mode = logging::LOG_TO_SYSTEM_DEBUG_LOG;
    else
      log_mode = kDefaultLoggingMode;
  } else {
    log_mode = logging::LOG_NONE;
  }
  return log_mode;
}

#if defined(OS_CHROMEOS)
namespace {
base::FilePath SetUpSymlinkIfNeeded(const base::FilePath& symlink_path,
                                    bool new_log) {
  DCHECK(!symlink_path.empty());

  // If not starting a new log, then just log through the existing
  // symlink, but if the symlink doesn't exist, create it.  If
  // starting a new log, then delete the old symlink and make a new
  // one to a fresh log file.
  base::FilePath target_path;
  bool symlink_exists = base::PathExists(symlink_path);
  if (new_log || !symlink_exists) {
    target_path = GenerateTimestampedName(symlink_path, base::Time::Now());

    // We don't care if the unlink fails; we're going to continue anyway.
    if (::unlink(symlink_path.value().c_str()) == -1) {
      if (symlink_exists) // only warn if we might expect it to succeed.
        DPLOG(WARNING) << "Unable to unlink " << symlink_path.value();
    }
    if (!base::CreateSymbolicLink(target_path, symlink_path)) {
      DPLOG(ERROR) << "Unable to create symlink " << symlink_path.value()
                   << " pointing at " << target_path.value();
    }
  } else {
    if (!base::ReadSymbolicLink(symlink_path, &target_path))
      DPLOG(ERROR) << "Unable to read symlink " << symlink_path.value();
  }
  return target_path;
}

void RemoveSymlinkAndLog(const base::FilePath& link_path,
                         const base::FilePath& target_path) {
  if (::unlink(link_path.value().c_str()) == -1)
    DPLOG(WARNING) << "Unable to unlink symlink " << link_path.value();
  if (::unlink(target_path.value().c_str()) == -1)
    DPLOG(WARNING) << "Unable to unlink log file " << target_path.value();
}

}  // anonymous namespace

base::FilePath GetSessionLogDir(const base::CommandLine& command_line) {
  base::FilePath log_dir;
  std::string log_dir_str;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->GetVar(env_vars::kSessionLogDir, &log_dir_str) &&
      !log_dir_str.empty()) {
    log_dir = base::FilePath(log_dir_str);
  } else if (command_line.HasSwitch(chromeos::switches::kLoginProfile)) {
    PathService::Get(chrome::DIR_USER_DATA, &log_dir);
    base::FilePath profile_dir;
    std::string login_profile_value =
        command_line.GetSwitchValueASCII(chromeos::switches::kLoginProfile);
    if (login_profile_value == chrome::kLegacyProfileDir ||
        login_profile_value == chrome::kTestUserProfileDir) {
      profile_dir = base::FilePath(login_profile_value);
    } else {
      // We could not use g_browser_process > profile_helper() here.
      std::string profile_dir_str = chrome::kProfileDirPrefix;
      profile_dir_str.append(login_profile_value);
      profile_dir = base::FilePath(profile_dir_str);
    }
    log_dir = log_dir.Append(profile_dir);
  }
  return log_dir;
}

base::FilePath GetSessionLogFile(const base::CommandLine& command_line) {
  return GetSessionLogDir(command_line).Append(GetLogFileName().BaseName());
}

void RedirectChromeLogging(const base::CommandLine& command_line) {
  if (chrome_logging_redirected_) {
    // TODO(nkostylev): Support multiple active users. http://crbug.com/230345
    LOG(WARNING) << "NOT redirecting logging for multi-profiles case.";
    return;
  }

  DCHECK(!chrome_logging_redirected_) <<
    "Attempted to redirect logging when it was already initialized.";

  // Redirect logs to the session log directory, if set.  Otherwise
  // defaults to the profile dir.
  base::FilePath log_path = GetSessionLogFile(command_line);

  // Creating symlink causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crbug.com/61143
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  // Always force a new symlink when redirecting.
  base::FilePath target_path = SetUpSymlinkIfNeeded(log_path, true);

  // ChromeOS always logs through the symlink, so it shouldn't be
  // deleted if it already exists.
  logging::LoggingSettings settings;
  settings.logging_dest = DetermineLogMode(command_line);
  settings.log_file = log_path.value().c_str();
  if (!logging::InitLogging(settings)) {
    DLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
    RemoveSymlinkAndLog(log_path, target_path);
  } else {
    chrome_logging_redirected_ = true;
  }
}

#endif  // OS_CHROMEOS

void InitChromeLogging(const base::CommandLine& command_line,
                       OldFileDeletionState delete_old_log_file) {
  DCHECK(!chrome_logging_initialized_) <<
    "Attempted to initialize logging when it was already initialized.";

  LoggingDestination logging_dest = DetermineLogMode(command_line);
  LogLockingState log_locking_state = LOCK_LOG_FILE;
  base::FilePath log_path;
#if defined(OS_CHROMEOS)
  base::FilePath target_path;
#endif

  // Don't resolve the log path unless we need to. Otherwise we leave an open
  // ALPC handle after sandbox lockdown on Windows.
  if ((logging_dest & LOG_TO_FILE) != 0) {
    log_path = GetLogFileName();

#if defined(OS_CHROMEOS)
    // For BWSI (Incognito) logins, we want to put the logs in the user
    // profile directory that is created for the temporary session instead
    // of in the system log directory, for privacy reasons.
    if (command_line.HasSwitch(chromeos::switches::kGuestSession))
      log_path = GetSessionLogFile(command_line);

    // On ChromeOS we log to the symlink.  We force creation of a new
    // symlink if we've been asked to delete the old log, since that
    // indicates the start of a new session.
    target_path = SetUpSymlinkIfNeeded(
        log_path, delete_old_log_file == logging::DELETE_OLD_LOG_FILE);

    // Because ChromeOS manages the move to a new session by redirecting
    // the link, it shouldn't remove the old file in the logging code,
    // since that will remove the newly created link instead.
    delete_old_log_file = logging::APPEND_TO_OLD_LOG_FILE;
#endif
  } else {
    log_locking_state = DONT_LOCK_LOG_FILE;
  }

  logging::LoggingSettings settings;
  settings.logging_dest = logging_dest;
  settings.log_file = log_path.value().c_str();
  settings.lock_log = log_locking_state;
  settings.delete_old = delete_old_log_file;
  bool success = logging::InitLogging(settings);

#if defined(OS_CHROMEOS)
  if (!success) {
    DPLOG(ERROR) << "Unable to initialize logging to " << log_path.value()
                << " (which should be a link to " << target_path.value() << ")";
    RemoveSymlinkAndLog(log_path, target_path);
    chrome_logging_failed_ = true;
    return;
  }
#else
  if (!success) {
    DPLOG(ERROR) << "Unable to initialize logging to " << log_path.value();
    chrome_logging_failed_ = true;
    return;
  }
#endif

  // Default to showing error dialogs.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kNoErrorDialogs))
    logging::SetShowErrorDialogs(true);

  // we want process and thread IDs because we have a lot of things running
  logging::SetLogItems(true,  // enable_process_id
                       true,  // enable_thread_id
                       true,  // enable_timestamp
                       false);  // enable_tickcount

  // We call running in unattended mode "headless", and allow
  // headless mode to be configured either by the Environment
  // Variable or by the Command Line Switch.  This is for
  // automated test purposes.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->HasVar(env_vars::kHeadless) ||
      command_line.HasSwitch(switches::kNoErrorDialogs))
    SuppressDialogs();

  // Use a minimum log level if the command line asks for one. Ignore this
  // switch if there's vlog level switch present too (as both of these switches
  // refer to the same underlying log level, and the vlog level switch has
  // already been processed inside logging::InitLogging). If there is neither
  // log level nor vlog level specified, then just leave the default level
  // (INFO).
  if (command_line.HasSwitch(switches::kLoggingLevel) &&
      logging::GetMinLogLevel() >= 0) {
    std::string log_level =
        command_line.GetSwitchValueASCII(switches::kLoggingLevel);
    int level = 0;
    if (base::StringToInt(log_level, &level) && level >= 0 &&
        level < LOG_NUM_SEVERITIES) {
      logging::SetMinLogLevel(level);
    } else {
      DLOG(WARNING) << "Bad log level: " << log_level;
    }
  }

#if defined(OS_WIN)
  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kChromeTraceProviderName);

  // Enable logging to the Windows Event Log.
  logging::SetEventSourceName(base::UTF16ToASCII(
      install_static::InstallDetails::Get().install_full_name()));
#endif

  base::StatisticsRecorder::InitLogOnShutdown();

  chrome_logging_initialized_ = true;
}

// This is a no-op, but we'll keep it around in case
// we need to do more cleanup in the future.
void CleanupChromeLogging() {
  if (chrome_logging_failed_)
    return;  // We failed to initiailize logging, no cleanup.

  DCHECK(chrome_logging_initialized_) <<
    "Attempted to clean up logging when it wasn't initialized.";

  CloseLogFile();

  chrome_logging_initialized_ = false;
  chrome_logging_redirected_ = false;
}

base::FilePath GetLogFileName() {
  std::string filename;
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (env->GetVar(env_vars::kLogFileName, &filename) && !filename.empty())
    return base::FilePath::FromUTF8Unsafe(filename);

  const base::FilePath log_filename(FILE_PATH_LITERAL("chrome_debug.log"));
  base::FilePath log_path;

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
base::FilePath GenerateTimestampedName(const base::FilePath& base_path,
                                       base::Time timestamp) {
  base::Time::Exploded time_deets;
  timestamp.LocalExplode(&time_deets);
  std::string suffix = base::StringPrintf("_%02d%02d%02d-%02d%02d%02d",
                                          time_deets.year,
                                          time_deets.month,
                                          time_deets.day_of_month,
                                          time_deets.hour,
                                          time_deets.minute,
                                          time_deets.second);
  return base_path.InsertBeforeExtensionASCII(suffix);
}

}  // namespace logging
