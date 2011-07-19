// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOOLBAR_RELOAD_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_TOOLBAR_RELOAD_BUTTON_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_button.h"

// ToolbarButton subclass which defers certain state changes when the mouse
// is hovering over it.

@interface ReloadButton : ToolbarButton {
 @private
  // Tracks whether the mouse is hovering for purposes of not making
  // unexpected state changes.
  BOOL isMouseInside_;
  scoped_nsobject<NSTrackingArea> trackingArea_;

  // Timer used when setting reload mode while the mouse is hovered.
  scoped_nsobject<NSTimer> pendingReloadTimer_;
}

// Returns YES if the mouse is currently inside the bounds.
- (BOOL)isMouseInside;

// Update the tag, and the image and tooltip to match.  If |anInt|
// matches the current tag, no action is taken.  |anInt| must be
// either |IDC_STOP| or |IDC_RELOAD|.
- (void)updateTag:(NSInteger)anInt;

// Update the button to be a reload button or stop button depending on
// |isLoading|.  If |force|, always sets the indicated mode.  If
// |!force|, and the mouse is over the button, defer the transition
// from stop button to reload button until the mouse has left the
// button, or until |pendingReloadTimer_| fires.  This prevents an
// inadvertent click _just_ as the state changes.
- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force;

@end

@interface ReloadButton (PrivateTestingMethods)
+ (void)setPendingReloadTimeout:(NSTimeInterval)seconds;
- (NSTrackingArea*)trackingArea;
@end

#endif  // CHROME_BROWSER_UI_COCOA_TOOLBAR_RELOAD_BUTTON_H_
