// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace media_router {

// static
void MediaRouterMetrics::RecordMediaRouterDialogOrigin(
    MediaRouterDialogOpenOrigin origin) {
  DCHECK(origin != MediaRouterDialogOpenOrigin::TOTAL_COUNT);
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Icon.Click.Location", static_cast<int>(origin),
      static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouteProviderWakeReason(
    MediaRouteProviderWakeReason reason) {
  DCHECK(reason != MediaRouteProviderWakeReason::TOTAL_COUNT);
  UMA_HISTOGRAM_ENUMERATION(
      "MediaRouter.Provider.WakeReason", static_cast<int>(reason),
      static_cast<int>(MediaRouteProviderWakeReason::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogPaint(
    const base::TimeDelta delta) {
  UMA_HISTOGRAM_TIMES("MediaRouter.Ui.Dialog.Paint",
                      delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogLoaded(
    const base::TimeDelta delta) {
  UMA_HISTOGRAM_TIMES("MediaRouter.Ui.Dialog.LoadedWithData",
                      delta);
}

}  // namespace media_router
