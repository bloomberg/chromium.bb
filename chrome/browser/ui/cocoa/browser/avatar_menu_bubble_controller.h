// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_BROWSER_AVATAR_MENU_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_BROWSER_AVATAR_MENU_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#import "chrome/browser/ui/cocoa/tracking_area.h"

class AvatarMenuModel;
class AvatarMenuModelObserver;
class Browser;
@class HoverImageButton;

// This window controller manages the bubble that displays a "menu" of profiles.
// It is brought open by clicking on the avatar icon in the window frame.
@interface AvatarMenuBubbleController : BaseBubbleController {
 @private
  // The model that contains the data from the backend.
  scoped_ptr<AvatarMenuModel> model_;

  // Observer for changes to the model.
  scoped_ptr<AvatarMenuModelObserver> bridge_;

  // Array of the below view controllers.
  scoped_nsobject<NSMutableArray> items_;
}

// Designated initializer. The browser is passed to the model for profile
// information.
- (id)initWithBrowser:(Browser*)parentBrowser
           anchoredAt:(NSPoint)point;

// Creats a new profile.
- (IBAction)newProfile:(id)sender;

// Switches to a given profile. |sender| is an AvatarMenuItemController.
- (IBAction)switchToProfile:(id)sender;

// Edits a given profile. |sender| is an AvatarMenuItemController.
- (IBAction)editProfile:(id)sender;

@end

////////////////////////////////////////////////////////////////////////////////

// This view controller manages the menu item XIB.
@interface AvatarMenuItemController : NSViewController {
 @private
  // The parent menu controller; owns this.
  __weak AvatarMenuBubbleController* controller_;

  // The AvatarMenuModel::item.model_index field.
  size_t modelIndex_;

  // Instance variables that back the outlets.
  __weak NSImageView* iconView_;
  __weak NSImageView* activeView_;
  __weak NSTextField* nameField_;
  // These two views sit on top of each other, and only one is visible at a
  // time. The editButton_ is visible when the mouse is over the item and the
  // emailField_ is visible otherwise.
  __weak NSTextField* emailField_;
  __weak NSButton* editButton_;
}
@property(readonly, nonatomic) size_t modelIndex;
@property(assign, nonatomic) IBOutlet NSImageView* iconView;
@property(assign, nonatomic) IBOutlet NSImageView* activeView;
@property(assign, nonatomic) IBOutlet NSTextField* nameField;
@property(assign, nonatomic) IBOutlet NSTextField* emailField;
@property(assign, nonatomic) IBOutlet NSButton* editButton;

// Designated initializer.
- (id)initWithModelIndex:(size_t)modelIndex
          menuController:(AvatarMenuBubbleController*)controller;

// Actions that are forwarded to the |controller_|.
- (IBAction)switchToProfile:(id)sender;
- (IBAction)editProfile:(id)sender;

// Highlights the subviews appropriately for a given event type from the switch
// profile button.
- (void)highlightForEventType:(NSEventType)type;

@end

////////////////////////////////////////////////////////////////////////////////

// Simple button cell to get tracking and mouse events forwarded back to the
// view controller for changing highlight style of the item subviews. This is
// an invisible button that underlays most of the menu item and is responsible
// for performing the switch profile action.
@interface AvatarMenuItemView : NSView {
 @private
  // The controller that manages this.
  __weak AvatarMenuItemController* viewController_;

  // Used to highlight the background on hover.
  ScopedCrTrackingArea trackingArea_;

  // Whether the mouse is inside the bounds of this view.
  BOOL mouseInside_;
}
@property(assign, nonatomic) IBOutlet AvatarMenuItemController* viewController;
@end

// Testing API /////////////////////////////////////////////////////////////////

@interface AvatarMenuBubbleController (ExposedForTesting)
- (id)initWithModel:(AvatarMenuModel*)model
             bridge:(AvatarMenuModelObserver*)bridge
       parentWindow:(NSWindow*)parent
         anchoredAt:(NSPoint)point;
- (void)performLayout;
- (NSMutableArray*)items;
@end


#endif  // CHROME_BROWSER_UI_COCOA_BROWSER_AVATAR_MENU_BUBBLE_CONTROLLER_H_
