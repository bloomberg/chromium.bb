// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_METRICS_UTIL_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_METRICS_UTIL_H_

#include "base/time/time.h"

namespace resource_coordinator {

// Records histograms when a tab is discarded.
void RecordTabDiscarded();

// Records histograms when a discarded tab is reloaded. |last_active_time| is
// the last time at which the tab was active. |discard_time| is the time at
// which the tab was discarded. |reload_time| is the time at which the tab was
// reloaded.
void RecordTabReloaded(base::TimeTicks last_active_time,
                       base::TimeTicks discard_time,
                       base::TimeTicks reload_time);

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_DISCARD_METRICS_UTIL_H_
