// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_metrics.h"

#include "base/metrics/histogram.h"

namespace banners {

void TrackDismissEvent(int event) {
  std::vector<int> codes;
  for (int i = DISMISS_EVENT_MIN + 1; i < DISMISS_EVENT_MAX; ++i) {
    codes.push_back(i);
  }
  DCHECK(std::find(codes.begin(), codes.end(), event) != codes.end());
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("AppBanners.DismissEvent", event, codes);
}

void TrackDisplayEvent(int event) {
  std::vector<int> codes;
  for (int i = DISPLAY_EVENT_MIN + 1; i < DISPLAY_EVENT_MAX; ++i) {
    codes.push_back(i);
  }
  DCHECK(std::find(codes.begin(), codes.end(), event) != codes.end());
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("AppBanners.DisplayEvent", event, codes);
}

void TrackInstallEvent(int event) {
  std::vector<int> codes;
  for (int i = INSTALL_EVENT_MIN + 1; i < INSTALL_EVENT_MAX; ++i) {
    codes.push_back(i);
  }
  DCHECK(std::find(codes.begin(), codes.end(), event) != codes.end());
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("AppBanners.InstallEvent", event, codes);
}

}  // namespace banners
