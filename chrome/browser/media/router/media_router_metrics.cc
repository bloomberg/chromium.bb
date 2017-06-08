// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_metrics.h"

#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"

namespace {
// How long to wait between device counts metrics are recorded. Set to 1 hour.
const int kDeviceCountMetricThresholdMins = 60;
}  // namespace

namespace media_router {

MediaRouterMetrics::MediaRouterMetrics() : clock_(new base::DefaultClock()) {}
MediaRouterMetrics::~MediaRouterMetrics() = default;

// static
const char MediaRouterMetrics::kHistogramDialAvailableDeviceCount[] =
    "MediaRouter.Dial.AvailableDevicesCount";
const char MediaRouterMetrics::kHistogramDialKnownDeviceCount[] =
    "MediaRouter.Dial.KnownDevicesCount";
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

void MediaRouterMetrics::RecordDialDeviceCounts(size_t available_device_count,
                                                size_t known_device_count) {
  if (clock_->Now() - device_count_metrics_record_time_ <
      base::TimeDelta::FromMinutes(kDeviceCountMetricThresholdMins)) {
    return;
  }
  UMA_HISTOGRAM_COUNTS_100(kHistogramDialAvailableDeviceCount,
                           available_device_count);
  UMA_HISTOGRAM_COUNTS_100(kHistogramDialKnownDeviceCount, known_device_count);
  device_count_metrics_record_time_ = clock_->Now();
}

void MediaRouterMetrics::SetClockForTest(std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

}  // namespace media_router
