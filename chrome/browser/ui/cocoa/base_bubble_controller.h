// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_ptr.h"

@class InfoBubbleView;
class TabStripModelObserverBridge;

// Base class for bubble controllers. Manages a xib that contains an
// InfoBubbleWindow which contains an InfoBubbleView. Contains code to close
// the bubble window on clicks outside of the window, and the like.
// To use this class:
// 1. Create a new xib that contains a window. Change the window's class to
//    InfoBubbleWindow. Give it a child view that autosizes to the window's full
//    size, give it class InfoBubbleView. Make the controller the window's
//    delegate.
// 2. Create a subclass of BaseBubbleController.
// 3. Change the xib's File Owner to your subclass.
// 4. Hook up the File Owner's |bubble_| to the InfoBubbleView in the xib.
@interface BaseBubbleController : NSWindowController<NSWindowDelegate> {
 @private
  NSWindow* parentWindow_;  // weak
  NSPoint anchor_;
  IBOutlet InfoBubbleView* bubble_;  // to set arrow position
  // Bridge for tab change notifications.
  scoped_ptr<TabStripModelObserverBridge> tabStripObserverBridge_;

  // Non-nil only on 10.7+. Both weak, owned by AppKit.
  // A local event tap that will dismiss the bubble when a click is delivered
  // outside the window. This is needed because the window shares first
  // responder with its parent.
  id eventTap_;
  // A notification observer that gets triggered when any window resigns key.
  id resignationObserver_;
}

@property(nonatomic, readonly) NSWindow* parentWindow;
// The point in base screen coordinates at which the bubble should open and the
// arrow tip points.
@property(nonatomic, assign) NSPoint anchorPoint;
@property(nonatomic, readonly) InfoBubbleView* bubble;

// Creates a bubble. |nibPath| is just the basename, e.g. @"FirstRunBubble".
// |anchoredAt| is in screen space. You need to call -showWindow: to make the
// bubble visible. It will autorelease itself when the user dismisses the
// bubble.
// This is the designated initializer.
- (id)initWithWindowNibPath:(NSString*)nibPath
               parentWindow:(NSWindow*)parentWindow
                 anchoredAt:(NSPoint)anchoredAt;


// Creates a bubble. |nibPath| is just the basename, e.g. @"FirstRunBubble".
// |view| must be in a window. The bubble will point at |offset| relative to
// |view|'s lower left corner. You need to call -showWindow: to make the
// bubble visible. It will autorelease itself when the user dismisses the
// bubble.
- (id)initWithWindowNibPath:(NSString*)nibPath
             relativeToView:(NSView*)view
                     offset:(NSPoint)offset;


// For subclasses that do not load from a XIB, this will simply set the instance
// variables appropriately. This will also replace the |-[self window]|'s
// contentView with an instance of InfoBubbleView.
- (id)initWithWindow:(NSWindow*)theWindow
        parentWindow:(NSWindow*)parentWindow
          anchoredAt:(NSPoint)anchoredAt;

// Creates an autoreleased separator view with a given frame. The height of the
// frame is ignored.
- (NSBox*)separatorWithFrame:(NSRect)frame;

@end

// Methods for use by subclasses.
@interface BaseBubbleController (Protected)
// Registers event taps *after* the window is shown so that the bubble is
// dismissed when it resigns key. This only needs to be called if
// |-showWindow:| is overriden and does not call super. Noop on OSes <10.7.
- (void)registerKeyStateEventTap;
@end
