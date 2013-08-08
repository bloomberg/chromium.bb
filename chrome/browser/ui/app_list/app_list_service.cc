// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

namespace {

base::TimeDelta GetTimeFromOriginalProcessStart(
    const CommandLine& command_line) {
  std::string start_time_string =
      command_line.GetSwitchValueASCII(switches::kOriginalProcessStartTime);
  int64 remote_start_time;
  base::StringToInt64(start_time_string, &remote_start_time);
  return base::Time::Now() - base::Time::FromInternalValue(remote_start_time);
}

}  // namespace

// static
void AppListService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kLastAppListLaunchPing, 0);
  registry->RegisterIntegerPref(prefs::kAppListLaunchCount, 0);
  registry->RegisterInt64Pref(prefs::kLastAppListAppLaunchPing, 0);
  registry->RegisterIntegerPref(prefs::kAppListAppLaunchCount, 0);
  registry->RegisterStringPref(prefs::kAppListProfile, std::string());
  registry->RegisterBooleanPref(prefs::kRestartWithAppList, false);
}

// static
void AppListService::RecordShowTimings(const CommandLine& command_line) {
  // The presence of kOriginalProcessStartTime implies that another process
  // has sent us its command line to handle, ie: we are already running.
  if (command_line.HasSwitch(switches::kOriginalProcessStartTime)) {
    base::TimeDelta elapsed = GetTimeFromOriginalProcessStart(command_line);
    if (command_line.HasSwitch(switches::kFastStart))
      UMA_HISTOGRAM_LONG_TIMES("Startup.ShowAppListWarmStartFast", elapsed);
    else
      UMA_HISTOGRAM_LONG_TIMES("Startup.ShowAppListWarmStart", elapsed);
  } else {
    // base::CurrentProcessInfo::CreationTime() is only defined on some
    // platforms.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    UMA_HISTOGRAM_LONG_TIMES(
        "Startup.ShowAppListColdStart",
        base::Time::Now() - base::CurrentProcessInfo::CreationTime());
#endif
  }
}
