// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"

namespace media_router {

// static
const char MediaRouterMetrics::kHistogramIconClickLocation[] =
    "MediaRouter.Icon.Click.Location";
const char MediaRouterMetrics::kHistogramUiDialogPaint[] =
    "MediaRouter.Ui.Dialog.Paint";
const char MediaRouterMetrics::kHistogramUiDialogLoadedWithData[] =
    "MediaRouter.Ui.Dialog.LoadedWithData";
const char MediaRouterMetrics::kHistogramUiFirstAction[] =
    "MediaRouter.Ui.FirstAction";
const char MediaRouterMetrics::kHistogramRouteCreationOutcome[] =
    "MediaRouter.Route.CreationOutcome";

// static
void MediaRouterMetrics::RecordMediaRouterDialogOrigin(
    MediaRouterDialogOpenOrigin origin) {
  DCHECK_LT(static_cast<int>(origin),
            static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramIconClickLocation, static_cast<int>(origin),
      static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogPaint(
    const base::TimeDelta delta) {
  UMA_HISTOGRAM_TIMES(kHistogramUiDialogPaint, delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogLoaded(
    const base::TimeDelta delta) {
  UMA_HISTOGRAM_TIMES(kHistogramUiDialogLoadedWithData, delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterInitialUserAction(
    MediaRouterUserAction action) {
  DCHECK_LT(static_cast<int>(action),
            static_cast<int>(MediaRouterUserAction::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramUiFirstAction, static_cast<int>(action),
      static_cast<int>(MediaRouterUserAction::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordRouteCreationOutcome(
    MediaRouterRouteCreationOutcome outcome) {
  DCHECK_LT(static_cast<int>(outcome),
            static_cast<int>(MediaRouterRouteCreationOutcome::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramRouteCreationOutcome, static_cast<int>(outcome),
      static_cast<int>(MediaRouterRouteCreationOutcome::TOTAL_COUNT));
}

}  // namespace media_router
