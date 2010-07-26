// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_RELOAD_BUTTON_H_
#define CHROME_BROWSER_COCOA_RELOAD_BUTTON_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/scoped_nsobject.h"

// NSButton subclass which defers certain state changes when the mouse
// is hovering over it.

@interface ReloadButton : NSButton {
 @private
  // Tracks whether the mouse is hovering for purposes of not making
  // unexpected state changes.
  BOOL isMouseInside_;
  scoped_nsobject<NSTrackingArea> trackingArea_;

  // Set when reload mode is requested, but not forced, and the mouse
  // is hovering.
  BOOL pendingReloadMode_;
}

// Returns YES if the mouse is currently inside the bounds.
- (BOOL)isMouseInside;

// Update the button to be a reload button or stop button depending on
// |isLoading|.  If |force|, always sets the indicated mode.  If
// |!force|, and the mouse is over the button, defer the transition
// from stop button to reload button until the mouse has left the
// button.  This prevents an inadvertent click _just_ as the state
// changes.
- (void)setIsLoading:(BOOL)isLoading force:(BOOL)force;

@end

@interface ReloadButton (PrivateTestingMethods)
- (NSTrackingArea*)trackingArea;
@end

#endif  // CHROME_BROWSER_COCOA_RELOAD_BUTTON_H_
