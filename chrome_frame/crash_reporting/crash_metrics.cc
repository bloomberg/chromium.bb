// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/crash_reporting/crash_metrics.h"

#include "base/metrics/histogram.h"
#include "base/win/registry.h"
#include "chrome_frame/utils.h"

static const wchar_t kChromeFrameMetricsKey[] =
    L"Software\\Google\\ChromeFrameMetrics";

base::LazyInstance<CrashMetricsReporter>
    g_crash_metrics_instance_ = LAZY_INSTANCE_INITIALIZER;

wchar_t* CrashMetricsReporter::g_metric_names[LAST_METRIC] = {
  L"navigationcount",
  L"crashcount",
  L"chrome_frame_navigationcount",
  L"sessionid",
  L"channel_error",
};

CrashMetricsReporter* CrashMetricsReporter::GetInstance() {
  return &g_crash_metrics_instance_.Get();
}

bool CrashMetricsReporter::SetMetric(Metric metric, int value) {
  DCHECK(metric >= NAVIGATION_COUNT && metric <= LAST_METRIC);

  base::win::RegKey metric_key;
  LONG result = metric_key.Create(HKEY_CURRENT_USER, kChromeFrameMetricsKey,
                                  KEY_SET_VALUE);
  if (result == ERROR_SUCCESS) {
    result = metric_key.WriteValue(g_metric_names[metric], value);
    if (result == ERROR_SUCCESS) {
      return true;
    } else {
      DLOG(ERROR) << "Failed to read ChromeFrame metric:"
        << g_metric_names[metric] << " error: " << result;
    }
  } else {
    DLOG(ERROR) << "Failed to create ChromeFrame metrics key. error: "
                << result;
  }
  return false;
}

int CrashMetricsReporter::GetMetric(Metric metric) {
  DCHECK(metric >= NAVIGATION_COUNT && metric <= LAST_METRIC);

  int ret = 0;
  base::win::RegKey metric_key;
  if (metric_key.Open(HKEY_CURRENT_USER, kChromeFrameMetricsKey,
                      KEY_QUERY_VALUE) == ERROR_SUCCESS) {
    metric_key.ReadValueDW(g_metric_names[metric],
                           reinterpret_cast<DWORD*>(&ret));
  }

  return ret;
}

int CrashMetricsReporter::IncrementMetric(Metric metric) {
  DCHECK(metric >= NAVIGATION_COUNT && metric <= LAST_METRIC);
  int metric_value = GetMetric(metric);
  metric_value++;
  SetMetric(metric, metric_value);
  return metric_value;
}

void CrashMetricsReporter::RecordCrashMetrics() {
  int navigation_count = GetMetric(NAVIGATION_COUNT);
  if (navigation_count > 0) {
    UMA_HISTOGRAM_COUNTS("ChromeFrame.HostNavigationCount", navigation_count);
    SetMetric(NAVIGATION_COUNT, 0);
  }

  int chrome_frame_navigation_count = GetMetric(CHROME_FRAME_NAVIGATION_COUNT);
  if (chrome_frame_navigation_count > 0) {
    UMA_HISTOGRAM_COUNTS("ChromeFrame.CFNavigationCount",
                         chrome_frame_navigation_count);
    SetMetric(CHROME_FRAME_NAVIGATION_COUNT, 0);
  }

  int crash_count = GetMetric(CRASH_COUNT);
  if (crash_count > 0) {
    UMA_HISTOGRAM_COUNTS("ChromeFrame.HostCrashCount", crash_count);
    SetMetric(CRASH_COUNT, 0);
  }

  int channel_error_count = GetMetric(CHANNEL_ERROR_COUNT);
  if (channel_error_count > 0) {
    UMA_HISTOGRAM_COUNTS("ChromeFrame.ChannelErrorCount", channel_error_count);
    SetMetric(CHANNEL_ERROR_COUNT, 0);
  }
}
