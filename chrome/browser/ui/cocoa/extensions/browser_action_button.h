// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/image_button_cell.h"

class Browser;
@class BrowserActionsController;
@class MenuController;
class ToolbarActionViewController;
class ToolbarActionViewDelegateBridge;

// Fired on each drag event while the user is moving the button.
extern NSString* const kBrowserActionButtonDraggingNotification;
// Fired when the user drops the button.
extern NSString* const kBrowserActionButtonDragEndNotification;

@interface BrowserActionButton : NSButton<NSMenuDelegate> {
 @private
  // Used to move the button and query whether a button is currently animating.
  base::scoped_nsobject<NSViewAnimation> moveAnimation_;

  // The controller that handles most non-view logic.
  ToolbarActionViewController* viewController_;

  // The bridge between the view controller and this object.
  scoped_ptr<ToolbarActionViewDelegateBridge> viewControllerDelegate_;

  // The context menu controller.
  base::scoped_nsobject<MenuController> contextMenuController_;

  // A substitute context menu to use in testing. We need this because normally
  // menu code is blocking, making it difficult to test.
  NSMenu* testContextMenu_;

  // The controller for the browser actions bar that owns this button. Weak.
  BrowserActionsController* browserActionsController_;

  // Whether the button is currently being dragged.
  BOOL isBeingDragged_;

  // Drag events could be intercepted by other buttons, so to make sure that
  // this is the only button moving if it ends up being dragged. This is set to
  // YES upon |mouseDown:|.
  BOOL dragCouldStart_;

  // If a drag is not currently in progress, this is the point where the mouse
  // down event occurred, and is used to prevent a drag from starting until it
  // moves at least kMinimumDragDistance. Once a drag begins, this is the point
  // at which the drag actually started.
  NSPoint dragStartPoint_;
}

// Init the button with the frame. Does not own either |view_controller| or
// |controller|.
- (id)initWithFrame:(NSRect)frame
     viewController:(ToolbarActionViewController*)viewController
         controller:(BrowserActionsController*)controller;

- (void)setFrame:(NSRect)frameRect animate:(BOOL)animate;

- (void)updateState;

// Called when the button is removed from the toolbar and will soon be deleted.
- (void)onRemoved;

- (BOOL)isAnimating;

// Returns the frame the button will occupy once animation is complete, or its
// current frame if it is not animating.
- (NSRect)frameAfterAnimation;

- (ToolbarActionViewController*)viewController;

// Returns a pointer to an autoreleased NSImage with the badge, shadow and
// cell image drawn into it.
- (NSImage*)compositedImage;

@property(readonly, nonatomic) BOOL isBeingDragged;

@end

@interface BrowserActionButton(TestingAPI)
// Sets a context menu to use for testing purposes.
- (void)setTestContextMenu:(NSMenu*)testContextMenu;
@end

@interface BrowserActionCell : ImageButtonCell {
 @private
  // The controller for the browser actions bar that owns the button. Weak.
  BrowserActionsController* browserActionsController_;

  // The view controller for the parent button. Weak.
  ToolbarActionViewController* viewController_;
}

@property(assign, nonatomic)
    BrowserActionsController* browserActionsController;
@property(readwrite, nonatomic) ToolbarActionViewController* viewController;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_BROWSER_ACTION_BUTTON_H_
