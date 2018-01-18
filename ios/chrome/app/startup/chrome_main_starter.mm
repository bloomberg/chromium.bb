// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/app/startup/chrome_main_starter.h"

#include "ios/chrome/app/startup/ios_chrome_main.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ChromeMainStarter

+ (std::unique_ptr<IOSChromeMain>)startChromeMain {
  return std::make_unique<IOSChromeMain>();
}

@end
