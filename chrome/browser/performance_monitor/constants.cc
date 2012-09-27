// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/constants.h"

namespace performance_monitor {

// The error message displayed when a metric's details are not found.
const char kMetricNotFoundError[] = "Metric details not found.";

// Any metric that is not associated with a specific activity will use this as
// its activity.
const char kProcessChromeAggregate[] = "chrome_aggregate";

// Tokens to retrieve state values from the database.

// Stores information about the previous chrome version.
const char kStateChromeVersion[] = "chrome_version";
// The prefix to the state of a profile's name, to prevent any possible naming
// collisions in the database.
const char kStateProfilePrefix[] = "profile";

}  // namespace performance_monitor
