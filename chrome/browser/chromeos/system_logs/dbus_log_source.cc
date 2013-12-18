// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/dbus_log_source.h"

#include "content/public/browser/browser_thread.h"
#include "dbus/dbus_statistics.h"

const char kDBusLogEntryShort[] = "dbus_summary";
const char kDBusLogEntryLong[] = "dbus_details";

namespace system_logs {

void DBusLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  SystemLogsResponse response;
  response[kDBusLogEntryShort] = dbus::statistics::GetAsString(
      dbus::statistics::SHOW_INTERFACE,
      dbus::statistics::FORMAT_ALL);
  response[kDBusLogEntryLong] = dbus::statistics::GetAsString(
      dbus::statistics::SHOW_METHOD,
      dbus::statistics::FORMAT_TOTALS);
  callback.Run(&response);
}

}  // namespace system_logs
