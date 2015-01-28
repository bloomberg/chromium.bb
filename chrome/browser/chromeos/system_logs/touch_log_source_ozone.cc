// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/touch_log_source.h"

#include "ash/touch/touch_hud_debug.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/process/launch.h"
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

// Callback for handing the outcome of GetTouchDeviceStatus(). Appends the
// collected log to the SystemLogsResponse map.
void OnStatusLogCollected(scoped_ptr<system_logs::SystemLogsResponse> response,
                          const system_logs::SysLogsSourceCallback& callback,
                          scoped_ptr<std::string> log) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  (*response)[kDeviceStatusLogDataKey] = *log;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(callback, base::Owned(response.release())));
}

// Collect touch HUD debug logs. This needs to be done on the UI thread.
void CollectTouchHudDebugLog(system_logs::SystemLogsResponse* response) {
  scoped_ptr<base::DictionaryValue> dictionary =
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<SystemLogsResponse> response(new SystemLogsResponse);
  CollectTouchHudDebugLog(response.get());

  // Collect touch device status logs.
  ui::OzonePlatform::GetInstance()->GetInputController()->GetTouchDeviceStatus(
      base::Bind(&OnStatusLogCollected, base::Passed(&response), callback));
}

}  // namespace system_logs
