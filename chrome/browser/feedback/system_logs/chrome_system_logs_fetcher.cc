// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"
#include "chrome/browser/feedback/system_logs/log_sources/memory_details_log_source.h"
#include "chrome/browser/feedback/system_logs/system_logs_fetcher.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/system_logs/command_line_log_source.h"
#include "chrome/browser/chromeos/system_logs/dbus_log_source.h"
#include "chrome/browser/chromeos/system_logs/debug_daemon_log_source.h"
#include "chrome/browser/chromeos/system_logs/device_event_log_source.h"
#include "chrome/browser/chromeos/system_logs/lsb_release_log_source.h"
#include "chrome/browser/chromeos/system_logs/touch_log_source.h"
#endif

namespace system_logs {

SystemLogsFetcher* BuildChromeSystemLogsFetcher() {
  const bool scrub_data = true;
  SystemLogsFetcher* fetcher = new SystemLogsFetcher(scrub_data);

  fetcher->AddSource(base::MakeUnique<ChromeInternalLogSource>());
  fetcher->AddSource(base::MakeUnique<CrashIdsSource>());
  fetcher->AddSource(base::MakeUnique<MemoryDetailsLogSource>());

#if defined(OS_CHROMEOS)
  fetcher->AddSource(base::MakeUnique<CommandLineLogSource>());
  fetcher->AddSource(base::MakeUnique<DBusLogSource>());
  fetcher->AddSource(base::MakeUnique<DeviceEventLogSource>());
  fetcher->AddSource(base::MakeUnique<LsbReleaseLogSource>());
  fetcher->AddSource(base::MakeUnique<TouchLogSource>());

  // Debug Daemon data source - currently only this data source supports
  // the scrub_data parameter, but the others still get scrubbed by
  // SystemLogsFetcher.
  fetcher->AddSource(base::MakeUnique<DebugDaemonLogSource>(scrub_data));
#endif

  return fetcher;
}

}  // namespace system_logs
