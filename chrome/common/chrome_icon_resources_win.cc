// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_icon_resources_win.h"

namespace icon_resources {

// The indices here must be consistent with the order of icons in chrome_exe.rc.

#if defined(GOOGLE_CHROME_BUILD)
const int kApplicationIndex = 0;
const int kApplication2Index = 1;
const int kApplication3Index = 2;
const int kApplication4Index = 3;
const int kSxSApplicationIndex = 4;
const int kAppLauncherIndex = 5;
const int kSxSAppLauncherIndex = 6;
const int kInstallPackagedAppIndex = 7;

#else
const int kApplicationIndex = 0;
const int kAppLauncherIndex = 1;
const int kInstallPackagedAppIndex = 2;

// For google_chrome_sxs_distribution.cc to compile in a dev build only.
const int kSxSApplicationIndex = -1;
const int kSxSAppLauncherIndex = -1;
#endif

}  // namespace icon_resources
