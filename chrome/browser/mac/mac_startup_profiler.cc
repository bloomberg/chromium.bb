// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/mac_startup_profiler.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "components/startup_metric_utils/startup_metric_utils.h"

// static
MacStartupProfiler* MacStartupProfiler::GetInstance() {
  return Singleton<MacStartupProfiler>::get();
}

MacStartupProfiler::MacStartupProfiler() : recorded_metrics_(false) {
}

MacStartupProfiler::~MacStartupProfiler() {
}

void MacStartupProfiler::Profile(Location location) {
  profiled_times_[location] = base::Time::Now();
}

void MacStartupProfiler::RecordMetrics() {
  const base::Time* main_entry_time =
      startup_metric_utils::MainEntryPointTime();
  DCHECK(main_entry_time);
  DCHECK(!recorded_metrics_);

  recorded_metrics_ = true;

  for (std::map<Location, base::Time>::const_iterator it =
           profiled_times_.begin();
       it != profiled_times_.end();
       ++it) {
    const base::Time& location_time = it->second;
    base::TimeDelta delta = location_time - *main_entry_time;
    RecordHistogram(it->first, delta);
  }
}

const std::string MacStartupProfiler::HistogramName(Location location) {
  std::string prefix("Startup.OSX.");
  switch (location) {
    case PRE_MAIN_MESSAGE_LOOP_START:
      return prefix + "PreMainMessageLoopStart";
    case AWAKE_FROM_NIB:
      return prefix + "AwakeFromNib";
    case POST_MAIN_MESSAGE_LOOP_START:
      return prefix + "PostMainMessageLoopStart";
    case PRE_PROFILE_INIT:
      return prefix + "PreProfileInit";
    case POST_PROFILE_INIT:
      return prefix + "PostProfileInit";
    case WILL_FINISH_LAUNCHING:
      return prefix + "WillFinishLaunching";
    case DID_FINISH_LAUNCHING:
      return prefix + "DockIconWillFinishBouncing";
  }
}

void MacStartupProfiler::RecordHistogram(Location location,
                                         const base::TimeDelta& delta) {
  const std::string name(HistogramName(location));
  base::TimeDelta min = base::TimeDelta::FromMilliseconds(10);
  base::TimeDelta max = base::TimeDelta::FromMinutes(1);
  int bucket_count = 100;

  // No need to cache the histogram pointers, since each invocation of this
  // method will be the first and only usage of a histogram with that given
  // name.
  base::HistogramBase* histogram = base::Histogram::FactoryTimeGet(
      name,
      min,
      max,
      bucket_count,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->AddTime(delta);
}
