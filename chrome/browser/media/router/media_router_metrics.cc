// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace media_router {

// static
void MediaRouterMetrics::RecordMediaRouterDialogOrigin(
    MediaRouterDialogOpenOrigin origin) {
  UMA_HISTOGRAM_ENUMERATION("MediaRouter.Icon.Click.Location",
                            origin, media_router::TOTAL_COUNT);
}

}  // namespace media_router
