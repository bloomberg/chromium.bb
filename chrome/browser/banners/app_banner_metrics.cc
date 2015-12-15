// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_metrics.h"

#include "base/metrics/sparse_histogram.h"

namespace banners {

void TrackDismissEvent(int event) {
  DCHECK_LT(DISMISS_EVENT_MIN, event);
  DCHECK_LT(event, DISMISS_EVENT_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY("AppBanners.DismissEvent", event);
}

void TrackDisplayEvent(int event) {
  DCHECK_LT(DISPLAY_EVENT_MIN, event);
  DCHECK_LT(event, DISPLAY_EVENT_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY("AppBanners.DisplayEvent", event);
}

void TrackInstallEvent(int event) {
  DCHECK_LT(INSTALL_EVENT_MIN, event);
  DCHECK_LT(event, INSTALL_EVENT_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY("AppBanners.InstallEvent", event);
}

void TrackUserResponse(int event) {
  DCHECK_LT(USER_RESPONSE_MIN, event);
  DCHECK_LT(event, USER_RESPONSE_MAX);
  UMA_HISTOGRAM_SPARSE_SLOWLY("AppBanners.UserResponse", event);
}

}  // namespace banners
