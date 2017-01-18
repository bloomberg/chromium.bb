// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "chrome/browser/installable/installable_logging.h"

namespace banners {

const char kDismissEventHistogram[] = "AppBanners.DismissEvent";
const char kDisplayEventHistogram[] = "AppBanners.DisplayEvent";
const char kInstallEventHistogram[] = "AppBanners.InstallEvent";
const char kMinutesHistogram[] =
    "AppBanners.MinutesFromFirstVisitToBannerShown";
const char kUserResponseHistogram[] = "AppBanners.UserResponse";
const char kBeforeInstallEventHistogram[] = "AppBanners.BeforeInstallEvent";
const char kInstallableStatusCodeHistogram[] =
    "AppBanners.InstallableStatusCode";

void TrackDismissEvent(int event) {
  DCHECK_LT(DISMISS_EVENT_MIN, event);
  DCHECK_LT(event, DISMISS_EVENT_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY(kDismissEventHistogram, event);
}

void TrackDisplayEvent(int event) {
  DCHECK_LT(DISPLAY_EVENT_MIN, event);
  DCHECK_LT(event, DISPLAY_EVENT_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY(kDisplayEventHistogram, event);
}

void TrackInstallEvent(int event) {
  DCHECK_LT(INSTALL_EVENT_MIN, event);
  DCHECK_LT(event, INSTALL_EVENT_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY(kInstallEventHistogram, event);
}

void TrackMinutesFromFirstVisitToBannerShown(int minutes) {
  // Histogram ranges from 1 minute to the number of minutes in 21 days.
  // This is one more day than the decay length of time for site engagement,
  // and seven more days than the expiry of visits for the app banner
  // navigation heuristic.
  UMA_HISTOGRAM_CUSTOM_COUNTS(kMinutesHistogram, minutes, 1, 30240, 100);
}

void TrackUserResponse(int event) {
  DCHECK_LT(USER_RESPONSE_MIN, event);
  DCHECK_LT(event, USER_RESPONSE_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY(kUserResponseHistogram, event);
}

void TrackBeforeInstallEvent(int event) {
  DCHECK_LT(BEFORE_INSTALL_EVENT_MIN, event);
  DCHECK_LT(event, BEFORE_INSTALL_EVENT_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY(kBeforeInstallEventHistogram, event);
}

void TrackInstallableStatusCode(InstallableStatusCode code) {
  DCHECK_LE(NO_ERROR_DETECTED, code);
  DCHECK_LT(code, MAX_ERROR_CODE);
  if (code != IN_INCOGNITO) {
    // Do not log that we are in incognito to UMA.
    UMA_HISTOGRAM_SPARSE_SLOWLY(kInstallableStatusCodeHistogram, code);
  }
}

}  // namespace banners
