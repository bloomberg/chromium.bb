// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use
// of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_SYSTEM_METRICS_LOGGER_IMPL_H_
#define CHROME_BROWSER_METRICS_SYSTEM_METRICS_LOGGER_IMPL_H_

#include "base/basictypes.h"
#include "chrome/browser/metrics/system_metrics_logger.h"

class Profile;

// Wraps calls to UserMetrics::RecordAction() and the appropriate
// version of the UMA_HISTOGRAM_*_TIMES macros, based on the metric
// being logged

class SystemMetricsLoggerImpl : public SystemMetricsLogger {
 public:
  SystemMetricsLoggerImpl();
  ~SystemMetricsLoggerImpl();
  void RecordOverviewKeystroke(Profile *profile);
  void RecordOverviewExitMouse(Profile *profile);
  void RecordOverviewExitKeystroke(Profile *profile);
  void RecordWindowCycleKeystroke(Profile *profile);
  void RecordBootTime(int64 time);
  void RecordUpTime(int64 time);
 private:
  DISALLOW_COPY_AND_ASSIGN(SystemMetricsLoggerImpl);
};

#endif  // CHROME_BROWSER_METRICS_SYSTEM_METRICS_LOGGER_IMPL_H_
