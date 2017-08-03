// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"

namespace media_router {

MediaRouterMetrics::MediaRouterMetrics() {}
MediaRouterMetrics::~MediaRouterMetrics() = default;

// static
const char MediaRouterMetrics::kHistogramDialParsingError[] =
    "MediaRouter.Dial.ParsingError";
const char MediaRouterMetrics::kHistogramIconClickLocation[] =
    "MediaRouter.Icon.Click.Location";
const char MediaRouterMetrics::kHistogramMediaRouterCastingSource[] =
    "MediaRouter.Source.CastingSource";
const char MediaRouterMetrics::kHistogramMediaRouterFileFormat[] =
    "MediaRouter.Source.LocalFileFormat";
const char MediaRouterMetrics::kHistogramMediaRouterFileSize[] =
    "MediaRouter.Source.LocalFileSize";
const char MediaRouterMetrics::kHistogramRouteCreationOutcome[] =
    "MediaRouter.Route.CreationOutcome";
const char MediaRouterMetrics::kHistogramUiDialogPaint[] =
    "MediaRouter.Ui.Dialog.Paint";
const char MediaRouterMetrics::kHistogramUiDialogLoadedWithData[] =
    "MediaRouter.Ui.Dialog.LoadedWithData";
const char MediaRouterMetrics::kHistogramUiFirstAction[] =
    "MediaRouter.Ui.FirstAction";

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

// static
void MediaRouterMetrics::RecordMediaRouterCastingSource(MediaCastMode source) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(kHistogramMediaRouterCastingSource, source);
}

void MediaRouterMetrics::RecordMediaRouterFileFormat(
    const media::container_names::MediaContainerName format) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramMediaRouterFileFormat, format,
                            media::container_names::CONTAINER_MAX);
}

void MediaRouterMetrics::RecordMediaRouterFileSize(int64_t size) {
  UMA_HISTOGRAM_MEMORY_LARGE_MB(kHistogramMediaRouterFileSize, size);
}

// static
void MediaRouterMetrics::RecordDialParsingError(
    chrome::mojom::DialParsingError parsing_error) {
  DCHECK_LT(static_cast<int>(parsing_error),
            static_cast<int>(chrome::mojom::DialParsingError::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramDialParsingError, parsing_error,
      static_cast<int>(chrome::mojom::DialParsingError::TOTAL_COUNT));
}

}  // namespace media_router
