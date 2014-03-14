// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_UTILITIES_H_
#define CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_UTILITIES_H_

namespace banners {

// Records metrics for app banners.
void TrackDisplayEvent(int event);
void TrackDismissEvent(int event);
void TrackInstallEvent(int event);

}  // namespace banners

#endif  // CHROME_BROWSER_ANDROID_BANNERS_APP_BANNER_UTILITIES_H_
