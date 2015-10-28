// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace page_load_metrics {

PageLoadExtraInfo::PageLoadExtraInfo(
    const base::TimeDelta& first_background_time,
    const base::TimeDelta& first_foreground_time,
    bool started_in_foreground)
    : first_background_time(first_background_time),
      first_foreground_time(first_foreground_time),
      started_in_foreground(started_in_foreground) {}

}  // namespace page_load_metrics
