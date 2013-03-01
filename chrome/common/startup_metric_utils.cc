// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/startup_metric_utils.h"

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/time.h"

namespace {

// Mark as volatile to defensively make sure usage is thread-safe.
// Note that at the time of this writing, access is only on the UI thread.
volatile bool g_non_browser_ui_displayed = false;

const base::Time* MainEntryPointTimeInternal() {
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

bool g_main_entry_time_was_recorded = false;
bool g_startup_stats_collection_finished = false;
bool g_was_slow_startup = false;

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

// Return the time recorded by RecordMainEntryPointTime().
const base::Time MainEntryStartTime() {
  DCHECK(g_main_entry_time_was_recorded);
  return *MainEntryPointTimeInternal();
}

void OnBrowserStartupComplete() {
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
  UMA_HISTOGRAM_LONG_TIMES(
      "Startup.BrowserMessageLoopStartTimeFromMainEntry",
      startup_time_from_main_entry);

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
