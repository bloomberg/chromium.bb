// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/constants.h"

namespace performance_monitor {

// The key to insert/retrieve information about the chrome version from the
// database.
const char kStateChromeVersion[] = "chrome_version";
const char kMetricNotFoundError[] = "Mertic details not found.";
const char kProcessChromeAggregate[] = "chrome_aggregate";

const char kMetricCPUUsageName[] = "CPU Usage";
const char kMetricCPUUsageDescription[] = "The CPU usage measured in percent.";
const char kMetricCPUUsageUnits[] = "percent";
const double kMetricCPUUsageTickSize = 100.0;

const char kMetricPrivateMemoryUsageName[] = "Private Memory Usage";
const char kMetricPrivateMemoryUsageDescription[] =
    "The private memory usage measured in bytes.";
const char kMetricPrivateMemoryUsageUnits[] = "percent";
const double kMetricPrivateMemoryUsageTickSize = 10000000.0;


}  // namespace performance_monitor
