// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/startup_metric_utils/startup_metric_utils.h"

#include "base/containers/hash_tables.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/process/process_info.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/time/time.h"

namespace {

// Mark as volatile to defensively make sure usage is thread-safe.
// Note that at the time of this writing, access is only on the UI thread.
volatile bool g_non_browser_ui_displayed = false;

base::Time* MainEntryPointTimeInternal() {
  static base::Time main_start_time = base::Time::Now();
  return &main_start_time;
}

typedef base::hash_map<std::string,base::TimeDelta> SubsystemStartupTimeHash;

SubsystemStartupTimeHash* GetSubsystemStartupTimeHash() {
  static SubsystemStartupTimeHash* slow_startup_time_hash =
                                    new SubsystemStartupTimeHash;
  return slow_startup_time_hash;
}

base::Lock* GetSubsystemStartupTimeHashLock() {
  static base::Lock* slow_startup_time_hash_lock = new base::Lock;
  return slow_startup_time_hash_lock;
}

// Record time of main entry so it can be read from Telemetry performance
// tests.
// TODO(jeremy): Remove once crbug.com/317481 is fixed.
void RecordMainEntryTimeHistogram() {
  const int kLowWordMask = 0xFFFFFFFF;
  const int kLower31BitsMask = 0x7FFFFFFF;
  base::TimeDelta browser_main_entry_time_absolute =
      base::TimeDelta::FromMilliseconds(
          MainEntryPointTimeInternal()->ToInternalValue() / 1000.0);

  uint64 browser_main_entry_time_raw_ms =
      browser_main_entry_time_absolute.InMilliseconds();

  base::TimeDelta browser_main_entry_time_raw_ms_high_word =
      base::TimeDelta::FromMilliseconds(
          (browser_main_entry_time_raw_ms >> 32) & kLowWordMask);
  // Shift by one because histograms only support non-negative values.
  base::TimeDelta browser_main_entry_time_raw_ms_low_word =
      base::TimeDelta::FromMilliseconds(
          (browser_main_entry_time_raw_ms >> 1) & kLower31BitsMask);

  // A timestamp is a 64 bit value, yet histograms can only store 32 bits.
  HISTOGRAM_TIMES("Startup.BrowserMainEntryTimeAbsoluteHighWord",
      browser_main_entry_time_raw_ms_high_word);
  HISTOGRAM_TIMES("Startup.BrowserMainEntryTimeAbsoluteLowWord",
      browser_main_entry_time_raw_ms_low_word);
}

bool g_main_entry_time_was_recorded = false;
bool g_startup_stats_collection_finished = false;
bool g_was_slow_startup = false;

// Environment variable that stores the timestamp when the executable's main()
// function was entered.
const char kChromeMainTimeEnvVar[] = "CHROME_MAIN_TIME";

}  // namespace

namespace startup_metric_utils {

bool WasNonBrowserUIDisplayed() {
  return g_non_browser_ui_displayed;
}

void SetNonBrowserUIDisplayed() {
  g_non_browser_ui_displayed = true;
}

void RecordMainEntryPointTime() {
  DCHECK(!g_main_entry_time_was_recorded);
  g_main_entry_time_was_recorded = true;
  MainEntryPointTimeInternal();
}

void RecordExeMainEntryTime() {
  std::string exe_load_time =
      base::Int64ToString(base::Time::Now().ToInternalValue());
  scoped_ptr<base::Environment> env(base::Environment::Create());
  env->SetVar(kChromeMainTimeEnvVar, exe_load_time);
}

#if defined(OS_ANDROID)
void RecordSavedMainEntryPointTime(const base::Time& entry_point_time) {
  DCHECK(!g_main_entry_time_was_recorded);
  g_main_entry_time_was_recorded = true;
  *MainEntryPointTimeInternal() = entry_point_time;
}
#endif // OS_ANDROID

// Return the time recorded by RecordMainEntryPointTime().
const base::Time MainEntryStartTime() {
  DCHECK(g_main_entry_time_was_recorded);
  return *MainEntryPointTimeInternal();
}

void OnBrowserStartupComplete(bool is_first_run) {
  RecordMainEntryTimeHistogram();

  // Bail if uptime < 7 minutes, to filter out cases where Chrome may have been
  // autostarted and the machine is under io pressure.
  const int64 kSevenMinutesInMilliseconds =
      base::TimeDelta::FromMinutes(7).InMilliseconds();
  if (base::SysInfo::Uptime() < kSevenMinutesInMilliseconds) {
    g_startup_stats_collection_finished = true;
    return;
  }

  // The Startup.BrowserMessageLoopStartTime histogram recorded in
  // chrome_browser_main.cc exhibits instability in the field which limits its
  // usefulness in all scenarios except when we have a very large sample size.
  // Attempt to mitigate this with a new metric:
  // * Measure time from main entry rather than the OS' notion of process start
  //   time.
  // * Only measure launches that occur 7 minutes after boot to try to avoid
  //   cases where Chrome is auto-started and IO is heavily loaded.
  base::TimeDelta startup_time_from_main_entry =
      base::Time::Now() - MainEntryStartTime();
  if (is_first_run) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Startup.BrowserMessageLoopStartTimeFromMainEntry.FirstRun",
        startup_time_from_main_entry);
  } else {
    UMA_HISTOGRAM_LONG_TIMES(
        "Startup.BrowserMessageLoopStartTimeFromMainEntry",
        startup_time_from_main_entry);
  }

// CurrentProcessInfo::CreationTime() is currently only implemented on some
// platforms.
#if (defined(OS_MACOSX) && !defined(OS_IOS)) || defined(OS_WIN) || \
    defined(OS_LINUX)
  // Record timings between process creation, the main() in the executable being
  // reached and the main() in the shared library being reached.
  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string chrome_main_entry_time_string;
  if (env->GetVar(kChromeMainTimeEnvVar, &chrome_main_entry_time_string)) {
    // The time that the Chrome executable's main() function was entered.
    int64 chrome_main_entry_time_int = 0;
    if (base::StringToInt64(chrome_main_entry_time_string,
                            &chrome_main_entry_time_int)) {
      base::Time process_create_time = base::CurrentProcessInfo::CreationTime();
      base::Time exe_main_time =
          base::Time::FromInternalValue(chrome_main_entry_time_int);
      base::Time dll_main_time = MainEntryStartTime();

      // Process create to chrome.exe:main().
      UMA_HISTOGRAM_LONG_TIMES("Startup.LoadTime.ProcessCreateToExeMain",
                               exe_main_time - process_create_time);

      // chrome.exe:main() to chrome.dll:main().
      UMA_HISTOGRAM_LONG_TIMES("Startup.LoadTime.ExeMainToDllMain",
                               dll_main_time - exe_main_time);

      // Process create to chrome.dll:main().
      UMA_HISTOGRAM_LONG_TIMES("Startup.LoadTime.ProcessCreateToDllMain",
                               dll_main_time - process_create_time);
    }
  }
#endif

  // Record histograms for the subsystem times for startups > 10 seconds.
  const base::TimeDelta kTenSeconds = base::TimeDelta::FromSeconds(10);
  if (startup_time_from_main_entry < kTenSeconds) {
    g_startup_stats_collection_finished = true;
    return;
  }

  // If we got here this was what we consider to be a slow startup which we
  // want to record stats for.
  g_was_slow_startup = true;
}

void OnInitialPageLoadComplete() {
  if (!g_was_slow_startup)
    return;
  DCHECK(!g_startup_stats_collection_finished);

  const base::TimeDelta kStartupTimeMin(
      base::TimeDelta::FromMilliseconds(1));
  const base::TimeDelta kStartupTimeMax(base::TimeDelta::FromMinutes(5));
  static const size_t kStartupTimeBuckets = 100;

  // Set UMA flag for histograms outside chrome/ that can't use the
  // ScopedSlowStartupUMA class.
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram("Startup.SlowStartupNSSInit");
  if (histogram)
    histogram->SetFlags(base::HistogramBase::kUmaTargetedHistogramFlag);

  // Iterate over the stats recorded by ScopedSlowStartupUMA and create
  // histograms for them.
  base::AutoLock locker(*GetSubsystemStartupTimeHashLock());
  SubsystemStartupTimeHash* time_hash = GetSubsystemStartupTimeHash();
  for (SubsystemStartupTimeHash::iterator i = time_hash->begin();
      i != time_hash->end();
      ++i) {
    const std::string histogram_name = i->first;
    base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
        histogram_name,
        kStartupTimeMin,
        kStartupTimeMax,
        kStartupTimeBuckets,
        base::Histogram::kUmaTargetedHistogramFlag);
    counter->AddTime(i->second);
  }

  g_startup_stats_collection_finished = true;
}

const base::Time* MainEntryPointTime() {
  if (!g_main_entry_time_was_recorded)
    return NULL;
  return MainEntryPointTimeInternal();
}

ScopedSlowStartupUMA::~ScopedSlowStartupUMA() {
  if (g_startup_stats_collection_finished)
    return;

  base::AutoLock locker(*GetSubsystemStartupTimeHashLock());
  SubsystemStartupTimeHash* hash = GetSubsystemStartupTimeHash();
  // Only record the initial sample for a given histogram.
  if (hash->find(histogram_name_) !=  hash->end())
    return;

  (*hash)[histogram_name_] =
      base::TimeTicks::Now() - start_time_;
}

}  // namespace startup_metric_utils
