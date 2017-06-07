// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_TOUCH_BAR_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_TOUCH_BAR_H_

#import <Cocoa/Cocoa.h>

#import "ui/base/cocoa/touch_bar_forward_declarations.h"

class Browser;
@class BrowserWindowController;

// Provides a touch bar for the browser window. This class implements the
// NSTouchBarDelegate and handles the items in the touch bar.
@interface BrowserWindowTouchBar : NSObject<NSTouchBarDelegate>
// True is the current page is loading. Used to determine if a stop or reload
// button should be provided.
@property(nonatomic, assign) BOOL isPageLoading;

// True if the current page is starred. Used by star touch bar button.
@property(nonatomic, assign) BOOL isStarred;

// Designated initializer.
- (instancetype)initWithBrowser:(Browser*)browser
        browserWindowController:(BrowserWindowController*)bwc;

// Creates and returns a touch bar for the browser window.
- (NSTouchBar*)makeTouchBar;

@end

#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_WINDOW_TOUCH_BAR_H_
