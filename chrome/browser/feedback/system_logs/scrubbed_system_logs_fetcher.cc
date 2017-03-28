// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/scrubbed_system_logs_fetcher.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system_logs/command_line_log_source.h"
#include "chrome/browser/chromeos/system_logs/dbus_log_source.h"
#include "chrome/browser/chromeos/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/chromeos/system_logs/device_event_log_source.h"
#include "chrome/browser/chromeos/system_logs/lsb_release_log_source.h"
#include "chrome/browser/chromeos/system_logs/touch_log_source.h"
#endif

using content::BrowserThread;

namespace system_logs {

ScrubbedSystemLogsFetcher::ScrubbedSystemLogsFetcher() {
  data_sources_.push_back(base::MakeUnique<ChromeInternalLogSource>());
  data_sources_.push_back(base::MakeUnique<CrashIdsSource>());
  data_sources_.push_back(base::MakeUnique<MemoryDetailsLogSource>());

#if defined(OS_CHROMEOS)
  data_sources_.push_back(base::MakeUnique<CommandLineLogSource>());
  data_sources_.push_back(base::MakeUnique<DBusLogSource>());
  data_sources_.push_back(base::MakeUnique<DeviceEventLogSource>());
  data_sources_.push_back(base::MakeUnique<LsbReleaseLogSource>());
  data_sources_.push_back(base::MakeUnique<TouchLogSource>());

  // Debug Daemon data source - currently only this data source supports
  // the scrub_data parameter but all others get processed by Rewrite()
  // as well.
  const bool scrub_data = true;
  data_sources_.push_back(base::MakeUnique<DebugDaemonLogSource>(scrub_data));
#endif

  num_pending_requests_ = data_sources_.size();
}

ScrubbedSystemLogsFetcher::~ScrubbedSystemLogsFetcher() {
}

void ScrubbedSystemLogsFetcher::Rewrite(const std::string& source_name,
                                        SystemLogsResponse* response) {
  for (auto& element : *response)
    element.second = anonymizer_.Anonymize(element.second);
}

}  // namespace system_logs
