// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_FULLSCREEN_MODE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_FULLSCREEN_MODE_CONTROLLER_H_

#include <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#import "ui/base/cocoa/tracking_area.h"

@class BrowserWindowController;

// This class is responsible for managing the menu bar and tabstrip animation
// when in --enable-simplified-fullscreen. By default, in fullscreen, only the
// toolbar and not the tabstrip are visible. When the user mouses near the top
// of the screen, then the full tabstrip becomes available. If the user mouses
// to the very top of the screen, the menubar also becomes visible.
//
// There is one instance of this class per BrowserWindowController, and it is
// created when fullscreen is being entered and is destroyed when fullscreen
// is exited.
@interface FullscreenModeController : NSObject<NSAnimationDelegate> {
 @private
  enum FullscreenToolbarState {
    kFullscreenToolbarOnly,
    kFullscreenToolbarAndTabstrip,
  };

  // The browser for which this is managing fullscreen. Weak, owns self.
  BrowserWindowController* controller_;

  // The tracking area used to observe the top region of the fullscren window,
  // to initiate the animations to bring down the tabstrip.
  ui::ScopedCrTrackingArea trackingArea_;

  // The animation that is either showing or hiding the tabstrip. Nil when no
  // animation is running.
  base::scoped_nsobject<NSAnimation> animation_;

  // The current and destination states of |animation_|. When no animation is
  // running, these values are equal.
  FullscreenToolbarState destinationState_;
  FullscreenToolbarState currentState_;

  // A Carbon event handler that tracks the revealed fraction of the menu bar.
  EventHandlerRef menuBarTrackingHandler_;

  // A fraction in the range [0.0, 1.0] that indicates how much of the
  // menu bar is visible. Updated via |menuBarTrackingHandler_|.
  CGFloat menuBarRevealFraction_;
}

// Designated initializer. Must be called after making the window fullscreen.
- (id)initWithBrowserWindowController:(BrowserWindowController*)bwc;

// Returns the pixel height of the menu bar, adjusted for fractional visibility.
- (CGFloat)menuBarHeight;

@end

#endif  // CHROME_BROWSER_UI_COCOA_FULLSCREEN_MODE_CONTROLLER_H_
