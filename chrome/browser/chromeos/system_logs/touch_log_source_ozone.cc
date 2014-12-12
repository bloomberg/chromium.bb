// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/touch_log_source.h"

#include "ash/touch/touch_hud_debug.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "components/feedback/feedback_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

const char kHUDLogDataKey[] = "hud_log";

void GetTouchLogsOzone(system_logs::SystemLogsResponse* response) {
  scoped_ptr<base::DictionaryValue> dictionary =
      ash::TouchHudDebug::GetAllAsDictionary();
  if (!dictionary->empty()) {
    std::string touch_log;
    JSONStringValueSerializer json(&touch_log);
    json.set_pretty_print(true);
    if (json.Serialize(*dictionary) && !touch_log.empty())
      (*response)[kHUDLogDataKey] = touch_log;
  }

  // TODO(sheckylin): Add ozone touch log collection implementation. See
  // http://crbug.com/400022.
  NOTIMPLEMENTED();
}

}  // namespace

namespace system_logs {

void TouchLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse* response = new SystemLogsResponse;
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE, base::Bind(&GetTouchLogsOzone, response),
      base::Bind(callback, base::Owned(response)));
}

}  // namespace system_logs
