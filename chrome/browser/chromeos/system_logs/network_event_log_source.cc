// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/network_event_log_source.h"

#include "base/message_loop/message_loop.h"
#include "chromeos/network/network_event_log.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

const char kNetworkEventLogEntry[] = "network_event_log";

void NetworkEventLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<SystemLogsResponse> response(new SystemLogsResponse);
  const int kMaxNetworkEventsForAboutSystem = 400;
  (*response)[kNetworkEventLogEntry] =
      chromeos::network_event_log::GetAsString(
          chromeos::network_event_log::OLDEST_FIRST,
          "time,file,level,desc",
          chromeos::network_event_log::kDefaultLogLevel,
          kMaxNetworkEventsForAboutSystem);
  callback.Run(response.get());
}

}  // namespace system_logs
