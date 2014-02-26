// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_ICON_RESOURCES_WIN_H_
#define CHROME_COMMON_CHROME_ICON_RESOURCES_WIN_H_

namespace icon_resources {

// This file contains the indices of icon resources in chrome_exe.rc.

// The Chrome/Chromium application icon.
extern const int kApplicationIndex;
#if defined(GOOGLE_CHROME_BUILD)
extern const int kApplication2Index;
extern const int kApplication3Index;
extern const int kApplication4Index;
#endif

// The Chrome Canary application icon.
extern const int kSxSApplicationIndex;

// The Chrome/Chromium App Launcher icon.
extern const int kAppLauncherIndex;

// The Chrome App Launcher Canary icon.
extern const int kSxSAppLauncherIndex;

// The icon for installing a Chrome packaged app.
extern const int kInstallPackagedAppIndex;

}  // namespace icon_resources

#endif  // CHROME_COMMON_CHROME_ICON_RESOURCES_WIN_H_
