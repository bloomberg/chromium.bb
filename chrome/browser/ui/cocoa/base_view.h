// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BASE_VIEW_H_
#define CHROME_BROWSER_UI_COCOA_BASE_VIEW_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "ui/gfx/rect.h"

// A view that provides common functionality that many views will need:
// - Automatic registration for mouse-moved events.
// - Funneling of mouse and key events to two methods
// - Coordinate conversion utilities

@interface BaseView : NSView {
 @private
  NSTrackingArea *trackingArea_;
  BOOL dragging_;
  scoped_nsobject<NSEvent> pendingExitEvent_;
}

- (id)initWithFrame:(NSRect)frame;

// Override these methods in a subclass.
- (void)mouseEvent:(NSEvent *)theEvent;
- (void)keyEvent:(NSEvent *)theEvent;

// Useful rect conversions (doing coordinate flipping)
- (gfx::Rect)flipNSRectToRect:(NSRect)rect;
- (NSRect)flipRectToNSRect:(gfx::Rect)rect;

@end

// A notification that a view may issue when it receives first responder status.
// The name is |kViewDidBecomeFirstResponder|, the object is the view, and the
// NSSelectionDirection is wrapped in an NSNumber under the key
// |kSelectionDirection|.
extern NSString* kViewDidBecomeFirstResponder;
extern NSString* kSelectionDirection;

#endif  // CHROME_BROWSER_UI_COCOA_BASE_VIEW_H_
