// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/constants.h"

namespace performance_monitor {

const char kMetricNotFoundError[] = "Mertic details not found.";
const char kProcessChromeAggregate[] = "chrome_aggregate";
const char kSampleMetricDescription[] = "A sample metric.";
const char kSampleMetricName[] = "SAMPLE";

// The state token to track Chrome versions in the database.
const char kStateChromeVersion[] = "chrome_version";

// The state token to precede a profile's name, to prevent any possible naming
// collisions in the database.
const char kStateProfilePrefix[] = "profile";

}  // namespace performance_monitor
