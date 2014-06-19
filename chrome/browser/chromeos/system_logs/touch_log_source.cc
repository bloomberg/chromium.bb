// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/touch_log_source.h"

#include "ash/touch/touch_hud_debug.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/process/launch.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const char kHUDLogDataKey[] = "hud_log";

void GetTouchLogs(system_logs::SystemLogsResponse* response) {
  scoped_ptr<base::DictionaryValue> dictionary =
      ash::TouchHudDebug::GetAllAsDictionary();
  if (!dictionary->empty()) {
    std::string touch_log;
    JSONStringValueSerializer json(&touch_log);
    json.set_pretty_print(true);
    if (json.Serialize(*dictionary) && !touch_log.empty())
      (*response)[kHUDLogDataKey] = touch_log;
  }

  std::vector<std::pair<std::string, CommandLine> > commands;
  CommandLine command =
    CommandLine(base::FilePath("/opt/google/input/inputcontrol"));
  command.AppendArg("--status");
  commands.push_back(std::make_pair("hack-33025-touchpad", command));

  command =
    CommandLine(base::FilePath("/opt/google/input/cmt_feedback"));
  commands.push_back(std::make_pair("hack-33025-touchpad_activity", command));

  command = CommandLine(
      base::FilePath("/opt/google/input/evdev_feedback"));
  commands.push_back(
      std::make_pair("hack-33025-touchscreen_activity", command));

  for (size_t i = 0; i < commands.size(); ++i) {
    std::string output;
    base::GetAppOutput(commands[i].second, &output);
    (*response)[commands[i].first] = output;
  }
}

}  // namespace

namespace system_logs {

TouchLogSource::TouchLogSource() {
}

TouchLogSource::~TouchLogSource() {
}

void TouchLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse* response = new SystemLogsResponse;
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&GetTouchLogs, response),
      base::Bind(callback, base::Owned(response)));
}

}  // namespace system_logs
