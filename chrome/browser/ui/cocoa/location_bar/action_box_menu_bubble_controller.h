// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ACTION_BOX_MENU_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ACTION_BOX_MENU_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"

class Browser;
@class HoverImageButton;

namespace ui {
class MenuModel;
}

// This window controller manages the action box popup menu.
@interface ActionBoxMenuBubbleController : BaseBubbleController {
 @private
  // The model that contains the data from the backend.
  scoped_ptr<ui::MenuModel> model_;

  // Array of the below view controllers.
  scoped_nsobject<NSMutableArray> items_;
}

// Designated initializer. |point| must be in screen coordinates.
- (id)initWithModel:(scoped_ptr<ui::MenuModel>)model
       parentWindow:(NSWindow*)parent
         anchoredAt:(NSPoint)point;

// Accesses the model.
- (ui::MenuModel*)model;

// Executes the action of a given menu item.
- (IBAction)itemSelected:(id)sender;

@end

////////////////////////////////////////////////////////////////////////////////

// This view controller manages the menu item XIB.
@interface ActionBoxMenuItemController : NSViewController {
 @private
  // The parent menu controller owns this.
  __weak ActionBoxMenuBubbleController* controller_;

  size_t modelIndex_;

  // Tracks whether this item is currently highlighted.
  BOOL isHighlighted_;

  // Instance variables that back the outlets.
  IBOutlet __weak NSImageView* iconView_;
  IBOutlet __weak NSTextField* nameField_;
}
@property(readonly, nonatomic) size_t modelIndex;
@property(assign, nonatomic) BOOL isHighlighted;
@property(assign, nonatomic)  NSTextField* nameField;

// Designated initializer.
- (id)initWithModelIndex:(size_t)modelIndex
          menuController:(ActionBoxMenuBubbleController*)controller;

// Highlights the subviews appropriately for a given event.
- (void)highlightForEvent:(NSEvent*)event;

// Execute the action of a given menu item.
- (IBAction)itemSelected:(id)sender;

@end

////////////////////////////////////////////////////////////////////////////////

// Simple button cell to get tracking and mouse events forwarded back to the
// view controller for changing highlight style of the item subviews. This is
// an invisible button that underlays most of the menu item and is responsible
// for performing the attached action.
@interface ActionBoxMenuItemView : NSView {
 @private
  // The controller that manages this.
  IBOutlet __weak ActionBoxMenuItemController* viewController_;

  // Used to highlight the background on hover.
  ScopedCrTrackingArea trackingArea_;
}

@property(assign, nonatomic) ActionBoxMenuItemController* viewController;

@end

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_ACTION_BOX_MENU_BUBBLE_CONTROLLER_H_
