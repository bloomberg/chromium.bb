// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a service that collects information about the user
// experience in order to help improve future versions of the app.

#ifndef CHROME_FRAME_CRASH_REPORTING_CRASH_METRICS_H_
#define CHROME_FRAME_CRASH_REPORTING_CRASH_METRICS_H_

#include "base/basictypes.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"

// This class provides functionality to track counters like successful page
// loads in the host browser, total number of crashes, page loads in chrome
// frame.
class CrashMetricsReporter {
 public:
  enum Metric {
    NAVIGATION_COUNT,
    CRASH_COUNT,
    CHROME_FRAME_NAVIGATION_COUNT,
    SESSION_ID,
    CHANNEL_ERROR_COUNT,
    LAST_METRIC,
  };
  // Returns the global instance of this class.
  static CrashMetricsReporter* GetInstance();

  // The following function pair return/set/increment the specified user
  // metrics value from the registry. These values are set under the
  // following key:-
  // HKCU\Software\\Google\\ChromeFrame\\UserMetrics
  int GetMetric(Metric metric);
  bool SetMetric(Metric metric, int value);
  int IncrementMetric(Metric metric);

  // Records the crash metrics counters like navigation count, crash count.
  void RecordCrashMetrics();

  bool active() const {
    return active_;
  }

  void set_active(bool active) {
    active_ = active;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<CrashMetricsReporter>;

  CrashMetricsReporter()
      : active_(false) {}
  virtual ~CrashMetricsReporter() {}

  // Indicates whether the crash metrics reporter instance is active.
  bool active_;

  static wchar_t* g_metric_names[LAST_METRIC];

  DISALLOW_COPY_AND_ASSIGN(CrashMetricsReporter);
};

#endif  // CHROME_FRAME_CRASH_REPORTING_CRASH_METRICS_H_
