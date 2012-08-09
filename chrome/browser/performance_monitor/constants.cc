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

}  // namespace performance_monitor
