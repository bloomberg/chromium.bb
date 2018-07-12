// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"

#include <memory>

#include "base/mac/availability.h"
#include "base/mac/mac_util.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#import "ui/base/cocoa/touch_bar_util.h"

@interface BrowserWindowTouchBarController () {
  // The browser associated with the touch bar.
  Browser* browser_;  // Weak.

  NSWindow* window_;  // Weak.

  base::scoped_nsobject<BrowserWindowDefaultTouchBar> defaultTouchBar_;
}
@end

@implementation BrowserWindowTouchBarController

- (instancetype)initWithBrowser:(Browser*)browser window:(NSWindow*)window {
  if ((self = [super init])) {
    defaultTouchBar_.reset([[BrowserWindowDefaultTouchBar alloc]
        initWithBrowser:browser
             controller:self]);
    window_ = window;
  }

  return self;
}

- (void)invalidateTouchBar {
  DCHECK([window_ respondsToSelector:@selector(setTouchBar:)]);
  [window_ performSelector:@selector(setTouchBar:) withObject:nil];
}

- (NSTouchBar*)makeTouchBar {
  return [defaultTouchBar_ makeTouchBar];
}

@end

@implementation BrowserWindowTouchBarController (ExposedForTesting)

- (BrowserWindowDefaultTouchBar*)defaultTouchBar {
  return defaultTouchBar_.get();
}

@end
