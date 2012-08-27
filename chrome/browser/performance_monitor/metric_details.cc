// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_details.h"

#include "base/logging.h"
#include "chrome/browser/performance_monitor/constants.h"

namespace performance_monitor {
namespace {

// Metric details follow.
// All metric details have the following constants:
// - Name
// - Description
// - Units
// - TickSize (the smallest possible maximum which will be viewed in the ui.)

// CPU Usage
const char kMetricCPUUsageName[] = "CPU Usage";
const char kMetricCPUUsageDescription[] = "The CPU usage measured in percent.";
const char kMetricCPUUsageUnits[] = "percent";
const double kMetricCPUUsageTickSize = 100.0;

// Private Memory Usage
const char kMetricPrivateMemoryUsageName[] = "Private Memory Usage";
const char kMetricPrivateMemoryUsageDescription[] =
    "The total private memory usage of all chrome processes measured in bytes.";
const char kMetricPrivateMemoryUsageUnits[] = "bytes";
const double kMetricPrivateMemoryUsageTickSize = 10000000.0;

// Shared Memory Usage
const char kMetricSharedMemoryUsageName[] = "Shared Memory Usage";
const char kMetricSharedMemoryUsageDescription[] =
    "The total shared memory usage of all chrome processes measured in bytes.";
const char kMetricSharedMemoryUsageUnits[] = "bytes";
const double kMetricSharedMemoryUsageTickSize = 10000000.0;

// Startup Time
const char kMetricStartupTimeName[] = "Startup Time";
const char kMetricStartupTimeDescription[] =
    "The startup time measured in microseconds";
const char kMetricStartupTimeUnits[] = "microseconds";
const double kMetricStartupTimeTickSize = 5000000;

// Test Startup Time
const char kMetricTestStartupTimeName[] = "Test Startup Time";
const char kMetricTestStartupTimeDescription[] =
    "The startup time of test startups measured in microseconds";
const char kMetricTestStartupTimeUnits[] = "microseconds";
const double kMetricTestStartupTimeTickSize = 5000000;

// Session Restore Time
const char kMetricSessionRestoreTimeName[] = "Session Restore Time";
const char kMetricSessionRestoreTimeDescription[] =
    "The session restore time measured in microseconds";
const char kMetricSessionRestoreTimeUnits[] = "microseconds";
const double kMetricSessionRestoreTimeTickSize = 5000000;

// Page Load Time
const char kMetricPageLoadTimeName[] = "Page Load Time";
const char kMetricPageLoadTimeDescription[] =
    "The amount of time taken to load a page measured in microseconds.";
const char kMetricPageLoadTimeUnits[] = "microseconds";
const double kMetricPageLoadTimeTickSize = 30000000.0;

// Network Bytes Read
const char kMetricNetworkBytesReadName[] = "Network Bytes Read";
const char kMetricNetworkBytesReadDescription[] =
    "The number of bytes read across the network.";
const char kMetricNetworkBytesReadUnits[] = "bytes";
const double kMetricNetworkBytesReadTickSize = 2000000.0;

// Keep this array synced with MetricTypes in the header file.
// TODO(mtytel): i18n.
const MetricDetails kMetricDetailsList[] = {
  {
    kMetricCPUUsageName,
    kMetricCPUUsageDescription,
    kMetricCPUUsageUnits,
    kMetricCPUUsageTickSize,
  },
  {
    kMetricPrivateMemoryUsageName,
    kMetricPrivateMemoryUsageDescription,
    kMetricPrivateMemoryUsageUnits,
    kMetricPrivateMemoryUsageTickSize
  },
  {
    kMetricSharedMemoryUsageName,
    kMetricSharedMemoryUsageDescription,
    kMetricSharedMemoryUsageUnits,
    kMetricSharedMemoryUsageTickSize
  },
  {
    kMetricStartupTimeName,
    kMetricStartupTimeDescription,
    kMetricStartupTimeUnits,
    kMetricStartupTimeTickSize
  },
  {
    kMetricTestStartupTimeName,
    kMetricTestStartupTimeDescription,
    kMetricTestStartupTimeUnits,
    kMetricTestStartupTimeTickSize
  },
  {
    kMetricSessionRestoreTimeName,
    kMetricSessionRestoreTimeDescription,
    kMetricSessionRestoreTimeUnits,
    kMetricSessionRestoreTimeTickSize
  },
  {
    kMetricPageLoadTimeName,
    kMetricPageLoadTimeDescription,
    kMetricPageLoadTimeUnits,
    kMetricPageLoadTimeTickSize
  },
  {
    kMetricNetworkBytesReadName,
    kMetricNetworkBytesReadDescription,
    kMetricNetworkBytesReadUnits,
    kMetricNetworkBytesReadTickSize
  }
};
COMPILE_ASSERT(ARRAYSIZE_UNSAFE(kMetricDetailsList) == METRIC_NUMBER_OF_METRICS,
               metric_names_incorrect_size);

}  // namespace

const MetricDetails* GetMetricDetails(MetricType metric_type) {
  DCHECK_GT(METRIC_NUMBER_OF_METRICS, metric_type);
  return &kMetricDetailsList[metric_type];
}

}  // namespace performance_monitor
