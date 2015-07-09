// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/rlz/rlz.h"

#import <UIKit/UIKit.h>

// static
rlz_lib::AccessPoint RLZTracker::ChromeOmnibox() {
  return UI_USER_INTERFACE_IDIOM() == UIUserInterfaceIdiomPhone
      ? rlz_lib::CHROME_IOS_OMNIBOX_MOBILE
      : rlz_lib::CHROME_IOS_OMNIBOX_TABLET;
}
