// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"

namespace resource_coordinator {

void RecordTabDiscarded() {
  static int discard_count = 0;
  UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.DiscardCount",
                              ++discard_count, 1, 1000, 50);
}

void RecordTabReloaded(base::TimeTicks last_active_time,
                       base::TimeTicks discard_time,
                       base::TimeTicks reload_time) {
  static int reload_count = 0;
  UMA_HISTOGRAM_CUSTOM_COUNTS("TabManager.Discarding.ReloadCount",
                              ++reload_count, 1, 1000, 50);
  auto discard_to_reload_time = reload_time - discard_time;
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "TabManager.Discarding.DiscardToReloadTime", discard_to_reload_time,
      base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(1), 100);
  if (!last_active_time.is_null()) {
    auto inactive_to_reload_time = reload_time - last_active_time;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "TabManager.Discarding.InactiveToReloadTime", inactive_to_reload_time,
        base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(1), 100);
  }
}

}  // namespace resource_coordinator
