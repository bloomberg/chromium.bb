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

enum StartupType {
  COLD_START,
  WARM_START,
  WARM_START_FAST,
};

base::Time GetOriginalProcessStartTime(const CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kOriginalProcessStartTime)) {
    std::string start_time_string =
        command_line.GetSwitchValueASCII(switches::kOriginalProcessStartTime);
    int64 remote_start_time;
    base::StringToInt64(start_time_string, &remote_start_time);
    return base::Time::FromInternalValue(remote_start_time);
  }

// base::CurrentProcessInfo::CreationTime() is only defined on some
// platforms.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
  return base::CurrentProcessInfo::CreationTime();
#else
  return base::Time();
#endif
}

StartupType GetStartupType(const CommandLine& command_line) {
  // The presence of kOriginalProcessStartTime implies that another process
  // has sent us its command line to handle, ie: we are already running.
  if (command_line.HasSwitch(switches::kOriginalProcessStartTime)) {
    return command_line.HasSwitch(switches::kFastStart) ?
        WARM_START_FAST : WARM_START;
  }
  return COLD_START;
}

// The time the process that caused the app list to be shown started. This isn't
// necessarily the currently executing process as we may be processing a command
// line given to a short-lived Chrome instance.
int64 g_original_process_start_time;

// The type of startup the the current app list show has gone through.
StartupType g_app_show_startup_type;

void RecordStartupInfo(StartupType startup_type, const base::Time& start_time) {
  g_original_process_start_time = start_time.ToInternalValue();
  g_app_show_startup_type = startup_type;
}

void RecordFirstPaintTiming() {
  base::Time start_time(
      base::Time::FromInternalValue(g_original_process_start_time));
  base::TimeDelta elapsed = base::Time::Now() - start_time;
  switch (g_app_show_startup_type) {
    case COLD_START:
      UMA_HISTOGRAM_LONG_TIMES("Startup.AppListFirstPaintColdStart", elapsed);
      break;
    case WARM_START:
      UMA_HISTOGRAM_LONG_TIMES("Startup.AppListFirstPaintWarmStart", elapsed);
      break;
    case WARM_START_FAST:
      UMA_HISTOGRAM_LONG_TIMES("Startup.AppListFirstPaintWarmStartFast",
                               elapsed);
      break;
  }
}

}  // namespace

// static
void AppListService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kLastAppListLaunchPing, 0);
  registry->RegisterIntegerPref(prefs::kAppListLaunchCount, 0);
  registry->RegisterInt64Pref(prefs::kLastAppListAppLaunchPing, 0);
  registry->RegisterIntegerPref(prefs::kAppListAppLaunchCount, 0);
  registry->RegisterStringPref(prefs::kAppListProfile, std::string());
  registry->RegisterBooleanPref(prefs::kAppLauncherIsEnabled, false);
  registry->RegisterBooleanPref(prefs::kAppLauncherHasBeenEnabled, false);
  registry->RegisterIntegerPref(prefs::kAppListEnableMethod,
                                ENABLE_NOT_RECORDED);
  registry->RegisterInt64Pref(prefs::kAppListEnableTime, 0);

#if defined(OS_MACOSX)
  registry->RegisterIntegerPref(prefs::kAppLauncherShortcutVersion, 0);
#endif

  // Identifies whether we should show the app launcher promo or not.
  // Note that a field trial also controls the showing, so the promo won't show
  // unless the pref is set AND the field trial is set to a proper group.
  registry->RegisterBooleanPref(prefs::kShowAppLauncherPromo, true);
}

// static
void AppListService::RecordShowTimings(const CommandLine& command_line) {
  base::Time start_time = GetOriginalProcessStartTime(command_line);
  if (start_time.is_null())
    return;

  base::TimeDelta elapsed = base::Time::Now() - start_time;
  StartupType startup = GetStartupType(command_line);
  switch (startup) {
    case COLD_START:
      UMA_HISTOGRAM_LONG_TIMES("Startup.ShowAppListColdStart", elapsed);
      break;
    case WARM_START:
      UMA_HISTOGRAM_LONG_TIMES("Startup.ShowAppListWarmStart", elapsed);
      break;
    case WARM_START_FAST:
      UMA_HISTOGRAM_LONG_TIMES("Startup.ShowAppListWarmStartFast", elapsed);
      break;
  }

  RecordStartupInfo(startup, start_time);
  Get(chrome::HOST_DESKTOP_TYPE_NATIVE)->SetAppListNextPaintCallback(
      RecordFirstPaintTiming);
}
