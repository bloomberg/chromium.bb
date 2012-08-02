// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/constants.h"

namespace performance_monitor {

// TODO(chebert): i18n on all constants.

// The error message displayed when a metric's details are not found.
const char kMetricNotFoundError[] = "Metric details not found.";

// Any metric that is not associated with a specific activity will use this as
// its activity.
const char kProcessChromeAggregate[] = "chrome_aggregate";

// The interval at which PerformanceMonitor performs its timed collections.
const int kGatherIntervalInMinutes = 2;

// Tokens to retrieve state values from the database.

// Stores information about the previous chrome version.
const char kStateChromeVersion[] = "chrome_version";
// The prefix to the state of a profile's name, to prevent any possible naming
// collisions in the database.
const char kStateProfilePrefix[] = "profile";

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
    "The private memory usage measured in bytes.";
const char kMetricPrivateMemoryUsageUnits[] = "percent";
const double kMetricPrivateMemoryUsageTickSize = 10000000.0;

}  // namespace performance_monitor
