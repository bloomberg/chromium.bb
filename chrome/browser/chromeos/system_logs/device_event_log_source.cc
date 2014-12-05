// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/device_event_log_source.h"

#include "base/message_loop/message_loop.h"
#include "chromeos/device_event_log.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

const char kNetworkEventLogEntry[] = "network_event_log";
const char kDeviceEventLogEntry[] = "device_event_log";

DeviceEventLogSource::DeviceEventLogSource() : SystemLogsSource("DeviceEvent") {
}

DeviceEventLogSource::~DeviceEventLogSource() {
}

void DeviceEventLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<SystemLogsResponse> response(new SystemLogsResponse);
  const int kMaxDeviceEventsForAboutSystem = 400;
  (*response)[kNetworkEventLogEntry] = chromeos::device_event_log::GetAsString(
      chromeos::device_event_log::OLDEST_FIRST, "time,file,level",
      chromeos::device_event_log::LOG_TYPE_NETWORK,
      chromeos::device_event_log::kDefaultLogLevel,
      kMaxDeviceEventsForAboutSystem);
  (*response)[kDeviceEventLogEntry] = chromeos::device_event_log::GetAsString(
      chromeos::device_event_log::OLDEST_FIRST, "time,file,type,level",
      chromeos::device_event_log::LOG_TYPE_NON_NETWORK,
      chromeos::device_event_log::LOG_LEVEL_DEBUG,
      kMaxDeviceEventsForAboutSystem);
  callback.Run(response.get());
}

}  // namespace system_logs
