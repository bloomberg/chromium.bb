// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_utilities.h"

#include <algorithm>
#include <string>

#include "base/metrics/histogram.h"
#include "chrome/browser/android/banners/app_banner_metrics_ids.h"

namespace banners {

void TrackDismissEvent(int event) {
  std::vector<int> codes;
  for (int i = DISMISS_MIN + 1; i < DISMISS_MAX; ++i) {
    codes.push_back(i);
  }
  DCHECK(std::find(codes.begin(), codes.end(), event) != codes.end());
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("AppBanners.DismissEvent", event, codes);
}

void TrackDisplayEvent(int event) {
  std::vector<int> codes;
  for (int i = DISPLAY_MIN + 1; i < DISPLAY_MAX; ++i) {
    codes.push_back(i);
  }
  DCHECK(std::find(codes.begin(), codes.end(), event) != codes.end());
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("AppBanners.DisplayEvent", event, codes);
}

void TrackInstallEvent(int event) {
  std::vector<int> codes;
  for (int i = INSTALL_MIN + 1; i < INSTALL_MAX; ++i) {
    codes.push_back(i);
  }
  DCHECK(std::find(codes.begin(), codes.end(), event) != codes.end());
  UMA_HISTOGRAM_CUSTOM_ENUMERATION("AppBanners.InstallEvent", event, codes);
}

}  // namespace banners
