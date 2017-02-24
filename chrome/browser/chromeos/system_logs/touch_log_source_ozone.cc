// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/touch_log_source.h"

#include <stddef.h>

#include "ash/touch/touch_hud_debug.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/task_scheduler/post_task.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

using content::BrowserThread;

namespace {

const char kHUDLogDataKey[] = "hud_log";

// The prefix "hack-33025" was historically chosen in http://crbug.com/139715.
// We continue to go with it in order to be compatible with the existing touch
// log processing toolchain.
const char kDeviceStatusLogDataKey[] = "hack-33025-touchpad";
const char kTouchpadEventLogDataKey[] = "hack-33025-touchpad_activity";
const char kTouchscreenEventLogDataKey[] = "hack-33025-touchscreen_activity";

// Directory for temp touch event logs.
const char kTouchEventLogDir[] = "/home/chronos/user/log";

// Prefixes of touch event logs.
const char kTouchpadGestureLogPrefix[] = "touchpad_activity_";
const char kTouchscreenLogPrefix[] = "evdev_input_events_";
const char kTouchpadEvdevLogPrefix[] = "cmt_input_events_";

// Binary paths.
const char kShellCommand[] = "/bin/sh";
const char kTarCommand[] = "/bin/tar cf -";
const char kUuencodeCommand[] = "/usr/bin/uuencode";

const int kMaxDeviceTouchEventLogs = 7;

// Clean up intermediate log files dumped during feedback creation.
void CleanupEventLog(std::unique_ptr<std::vector<base::FilePath>> log_paths) {
  for (size_t i = 0; i < log_paths->size(); ++i)
    base::DeleteFile((*log_paths)[i], false);
}

// Check for all known log paths and find the ones whose filenames match a
// prefix. Concatenate their filenames into one string. |max_log_count| is
// the maximum number of logs that we will collect.
//
// This is used to distinguish touchpad/mice logs from touchscreen logs.
std::string GetEventLogListOfOnePrefix(
    const std::vector<base::FilePath>& log_paths,
    const std::string& prefix,
    const int max_log_count) {
  int collected = 0;
  std::string log_list;
  for (size_t i = 0; i < log_paths.size(); ++i) {
    const std::string basename = log_paths[i].BaseName().value();
    if (base::StartsWith(basename, prefix, base::CompareCase::SENSITIVE)) {
      log_list.append(" " + log_paths[i].value());

      // Limit the max number of collected logs to shorten the log collection
      // process.
      if (++collected >= max_log_count)
        break;
    }
  }

  return log_list;
}

// Pack the collected event logs in a way that is compatible with the log
// analysis tools.
void PackEventLog(system_logs::SystemLogsResponse* response,
                  std::unique_ptr<std::vector<base::FilePath>> log_paths) {
  // Combine logs with a command line call that tars them up and uuencode the
  // result in one string. This is to be compatible with the X11 behavior.
  std::vector<std::pair<std::string, base::CommandLine>> commands;
  base::CommandLine command = base::CommandLine(base::FilePath(kShellCommand));
  command.AppendArg("-c");

  // Make a list that contains touchpad (and mouse) event logs only.
  const std::string touchpad_log_list =
      GetEventLogListOfOnePrefix(*log_paths, kTouchpadGestureLogPrefix,
                                 kMaxDeviceTouchEventLogs) +
      GetEventLogListOfOnePrefix(*log_paths, kTouchpadEvdevLogPrefix,
                                 kMaxDeviceTouchEventLogs);
  command.AppendArg(std::string(kTarCommand) + touchpad_log_list +
                    " 2>/dev/null | " + kUuencodeCommand +
                    " -m touchpad_activity_log.tar");

  commands.push_back(std::make_pair(kTouchpadEventLogDataKey, command));

  base::CommandLine ts_command =
      base::CommandLine(base::FilePath(kShellCommand));
  ts_command.AppendArg("-c");

  // Make a list that contains touchscreen event logs only.
  const std::string touchscreen_log_list = GetEventLogListOfOnePrefix(
      *log_paths, kTouchscreenLogPrefix, kMaxDeviceTouchEventLogs);
  ts_command.AppendArg(std::string(kTarCommand) + touchscreen_log_list +
                       " 2>/dev/null | " + kUuencodeCommand +
                       " -m touchscreen_activity_log.tar");

  commands.push_back(std::make_pair(kTouchscreenEventLogDataKey, ts_command));

  // For now only touchpad (and mouse) logs are actually collected.
  for (size_t i = 0; i < commands.size(); ++i) {
    std::string output;
    base::GetAppOutput(commands[i].second, &output);
    (*response)[commands[i].first] = output;
  }

  // Cleanup these temporary log files.
  base::PostTaskWithTraits(
      FROM_HERE,
      base::TaskTraits().MayBlock().WithPriority(
          base::TaskPriority::BACKGROUND),
      base::Bind(CleanupEventLog, base::Passed(&log_paths)));
}

// Callback for handing the outcome of GetTouchEventLog().
//
// This is the end of the whole touch log collection process.
void OnEventLogCollected(
    std::unique_ptr<system_logs::SystemLogsResponse> response,
    const system_logs::SysLogsSourceCallback& callback,
    std::unique_ptr<std::vector<base::FilePath>> log_paths) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We cannot eliminate these temporaries and inline these closures because the
  // compiler may call release() before get().
  const base::Closure pack_closure =
      base::Bind(&PackEventLog, base::Unretained(response.get()),
                 base::Passed(&log_paths));
  const base::Closure callback_closure =
      base::Bind(callback, base::Owned(response.release()));
  base::PostTaskWithTraitsAndReply(FROM_HERE,
                                   base::TaskTraits().MayBlock().WithPriority(
                                       base::TaskPriority::BACKGROUND),
                                   pack_closure, callback_closure);
}

// Callback for handing the outcome of GetTouchDeviceStatus().
//
// Appends the collected log to the SystemLogsResponse map. Also goes on to
// collect touch event logs.
void OnStatusLogCollected(
    std::unique_ptr<system_logs::SystemLogsResponse> response,
    const system_logs::SysLogsSourceCallback& callback,
    std::unique_ptr<std::string> log) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  (*response)[kDeviceStatusLogDataKey] = *log;

  // Collect touch event logs.
  const base::FilePath kBaseLogPath(kTouchEventLogDir);
  ui::OzonePlatform::GetInstance()->GetInputController()->GetTouchEventLog(
      kBaseLogPath,
      base::Bind(&OnEventLogCollected, base::Passed(&response), callback));
}

// Collect touch HUD debug logs. This needs to be done on the UI thread.
void CollectTouchHudDebugLog(system_logs::SystemLogsResponse* response) {
  std::unique_ptr<base::DictionaryValue> dictionary =
      ash::TouchHudDebug::GetAllAsDictionary();
  if (!dictionary->empty()) {
    std::string touch_log;
    JSONStringValueSerializer json(&touch_log);
    json.set_pretty_print(true);
    if (json.Serialize(*dictionary) && !touch_log.empty())
      (*response)[kHUDLogDataKey] = touch_log;
  }
}

}  // namespace

namespace system_logs {

void TouchLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!callback.is_null());

  std::unique_ptr<SystemLogsResponse> response(new SystemLogsResponse);
  CollectTouchHudDebugLog(response.get());

  // Collect touch device status logs.
  ui::OzonePlatform::GetInstance()->GetInputController()->GetTouchDeviceStatus(
      base::Bind(&OnStatusLogCollected, base::Passed(&response), callback));
}

}  // namespace system_logs
