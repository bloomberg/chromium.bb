// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LEGACY_SIZE_CLASS_SUPPORT_UTIL_H_
#define IOS_CHROME_BROWSER_UI_LEGACY_SIZE_CLASS_SUPPORT_UTIL_H_

#import "ios/chrome/browser/ui/UIView+SizeClassSupport.h"

// Temporary functions for supporting size classes on pre-iOS8 devices.  The
// return values are based on the bounds of the main UIScreen.
// TODO(crbug.com/519568): Remove once Chrome for iOS drops support for iOS7.
SizeClassIdiom CurrentWidthSizeClass();
SizeClassIdiom CurrentHeightSizeClass();

#endif  // IOS_CHROME_BROWSER_UI_LEGACY_SIZE_CLASS_SUPPORT_UTIL_H_
