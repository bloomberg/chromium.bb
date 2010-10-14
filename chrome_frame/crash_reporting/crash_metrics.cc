// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/crash_reporting/crash_metrics.h"

#include "base/metrics/histogram.h"
#include "base/registry.h"
#include "chrome_frame/utils.h"

static const wchar_t kChromeFrameMetricsKey[] =
    L"Software\\Google\\ChromeFrameMetrics";

base::LazyInstance<CrashMetricsReporter>
    g_crash_metrics_instance_(base::LINKER_INITIALIZED);

wchar_t* CrashMetricsReporter::g_metric_names[LAST_METRIC] = {
  L"navigationcount",
  L"crashcount",
  L"chrome_frame_navigationcount",
  L"sessionid",
};

CrashMetricsReporter* CrashMetricsReporter::GetInstance() {
  return &g_crash_metrics_instance_.Get();
}

bool CrashMetricsReporter::SetMetric(Metric metric, int value) {
  DCHECK(metric >= NAVIGATION_COUNT && metric <= LAST_METRIC);

  RegKey metric_key;
  if (metric_key.Create(HKEY_CURRENT_USER, kChromeFrameMetricsKey,
                        KEY_SET_VALUE)) {
    if (metric_key.WriteValue(g_metric_names[metric], value)) {
      return true;
    } else {
      DLOG(ERROR) << "Failed to read ChromeFrame metric:"
                  << g_metric_names[metric];
    }
  } else {
    DLOG(ERROR) << "Failed to create ChromeFrame metrics key";
  }
  return false;
}

int CrashMetricsReporter::GetMetric(Metric metric) {
  DCHECK(metric >= NAVIGATION_COUNT && metric <= LAST_METRIC);

  int ret = 0;
  RegKey metric_key;
  if (metric_key.Open(HKEY_CURRENT_USER, kChromeFrameMetricsKey,
                      KEY_QUERY_VALUE)) {
    int value = 0;
    if (metric_key.ReadValueDW(g_metric_names[metric],
                               reinterpret_cast<DWORD*>(&value))) {
      ret = value;
    }
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
    THREAD_SAFE_UMA_HISTOGRAM_COUNTS("ChromeFrame.HostNavigationCount",
                                     navigation_count);
    SetMetric(NAVIGATION_COUNT, 0);
  }

  int chrome_frame_navigation_count = GetMetric(CHROME_FRAME_NAVIGATION_COUNT);
  if (chrome_frame_navigation_count > 0) {
    THREAD_SAFE_UMA_HISTOGRAM_COUNTS("ChromeFrame.CFNavigationCount",
                                     chrome_frame_navigation_count);
    SetMetric(CHROME_FRAME_NAVIGATION_COUNT, 0);
  }

  int crash_count = GetMetric(CRASH_COUNT);
  if (crash_count > 0) {
    THREAD_SAFE_UMA_HISTOGRAM_COUNTS("ChromeFrame.HostCrashCount",
                                      crash_count);
    SetMetric(CRASH_COUNT, 0);
  }
}

