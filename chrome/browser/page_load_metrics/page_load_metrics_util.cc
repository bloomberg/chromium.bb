// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"

#include <algorithm>

#include "chrome/browser/page_load_metrics/page_load_metrics_observer.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"

namespace page_load_metrics {

bool WasStartedInForegroundOptionalEventInForeground(
    const base::Optional<base::TimeDelta>& event,
    const PageLoadExtraInfo& info) {
  return info.started_in_foreground && event &&
         (!info.first_background_time ||
          event.value() <= info.first_background_time.value());
}

bool WasParseInForeground(const base::Optional<base::TimeDelta>& parse_start,
                          const base::Optional<base::TimeDelta>& parse_stop,
                          const PageLoadExtraInfo& info) {
  if (!parse_start) {
    return false;
  }
  const bool incomplete_parse_in_foreground =
      !parse_stop && info.started_in_foreground && !info.first_background_time;

  return incomplete_parse_in_foreground ||
         WasStartedInForegroundOptionalEventInForeground(parse_stop, info);
}
}  // namespace page_load_metrics
