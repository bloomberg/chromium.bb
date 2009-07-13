// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use
// of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/system_metrics_logger_impl.h"

#include "base/basictypes.h"
#include "base/histogram.h"
#include "base/time.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/metrics/user_metrics.h"

SystemMetricsLoggerImpl::SystemMetricsLoggerImpl() {}

SystemMetricsLoggerImpl::~SystemMetricsLoggerImpl() {}

void SystemMetricsLoggerImpl::RecordOverviewKeystroke(Profile *profile) {
  UserMetrics::RecordAction(L"TabOverview_Keystroke", profile);
}

void SystemMetricsLoggerImpl::RecordOverviewExitMouse(Profile *profile) {
  UserMetrics::RecordAction(L"TabOverview_ExitMouse", profile);
}

void SystemMetricsLoggerImpl::RecordOverviewExitKeystroke(Profile *profile) {
  UserMetrics::RecordAction(L"TabOverview_ExitKeystroke", profile);
}

void SystemMetricsLoggerImpl::RecordWindowCycleKeystroke(Profile *profile) {
  UserMetrics::RecordAction(L"TabOverview_WindowCycleKeystroke", profile);
}

void SystemMetricsLoggerImpl::RecordBootTime(int64 time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("ChromeOS.Boot Time",
                             base::TimeDelta::FromMilliseconds(time),
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(1),
                             50);
}

void SystemMetricsLoggerImpl::RecordUpTime(int64 time) {
  UMA_HISTOGRAM_LONG_TIMES("ChromeOS.Uptime",
                           base::TimeDelta::FromSeconds(time));
}
