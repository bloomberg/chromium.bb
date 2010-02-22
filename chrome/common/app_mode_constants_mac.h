// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_APP_MODE_CONSTANTS_MAC_H_
#define CHROME_COMMON_APP_MODE_CONSTANTS_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

// This file contains constants which must be known both by the browser
// application and by the app mode loader (a.k.a. shim).

namespace app_mode {

// The ID under which app mode preferences will be recorded
// ("org.chromium.Chromium" or "com.google.Chrome").
extern const CFStringRef kAppPrefsID;

// The key under which to record the path to the (user-visible) application
// bundle; this key is recorded under the ID given by |kAppPrefsID|.
extern const CFStringRef kLastRunAppBundlePathPrefsKey;

}  // namespace app_mode

#endif  // CHROME_COMMON_APP_MODE_CONSTANTS_MAC_H_
