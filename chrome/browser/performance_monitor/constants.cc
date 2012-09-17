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

// The default interval at which PerformanceMonitor performs its timed
// collections; this can be overridden by using the kPerformanceMonitorGathering
// switch with an associated (positive integer) value.
const int kDefaultGatherIntervalInSeconds = 120;

// Tokens to retrieve state values from the database.

// Stores information about the previous chrome version.
const char kStateChromeVersion[] = "chrome_version";
// The prefix to the state of a profile's name, to prevent any possible naming
// collisions in the database.
const char kStateProfilePrefix[] = "profile";

}  // namespace performance_monitor
