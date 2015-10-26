// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WRENCH_MENU_WRENCH_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_WRENCH_MENU_WRENCH_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/has_weak_browser_pointer.h"
#import "ui/base/cocoa/menu_controller.h"

class BookmarkMenuBridge;
class Browser;
@class BrowserActionsContainerView;
@class BrowserActionsController;
@class MenuTrackedRootView;
class RecentTabsMenuModelDelegate;
@class ToolbarController;
@class WrenchMenuButtonViewController;
class AppMenuModel;

namespace wrench_menu_controller {
// The vertical offset of the wrench bubbles from the wrench menu button.
extern const CGFloat kWrenchBubblePointOffsetY;
}

namespace WrenchMenuControllerInternal {
class AcceleratorDelegate;
class ToolbarActionsBarObserverHelper;
class ZoomLevelObserver;
}  // namespace WrenchMenuControllerInternal

// The Wrench menu has a creative layout, with buttons in menu items. There is
// a cross-platform model for this special menu, but on the Mac it's easier to
// get spacing and alignment precisely right using a NIB. To do that, we
// subclass the generic MenuController implementation and special-case the two
// items that require specific layout and load them from the NIB.
//
// This object is owned by the ToolbarController and receives its NIB-based
// views using the shim view controller below.
@interface WrenchMenuController
    : MenuController<NSMenuDelegate, HasWeakBrowserPointer> {
 @private
  // Used to provide accelerators for the menu.
  scoped_ptr<WrenchMenuControllerInternal::AcceleratorDelegate>
      acceleratorDelegate_;

  // The model, rebuilt each time the |-menuNeedsUpdate:|.
  scoped_ptr<AppMenuModel> appMenuModel_;

  // Used to update icons in the recent tabs menu. This must be declared after
  // |appMenuModel_| so that it gets deleted first.
  scoped_ptr<RecentTabsMenuModelDelegate> recentTabsMenuModelDelegate_;

  // A shim NSViewController that loads the buttons from the NIB because ObjC
  // doesn't have multiple inheritance as this class is a MenuController.
  base::scoped_nsobject<WrenchMenuButtonViewController> buttonViewController_;

  // The browser for which this controller exists.
  Browser* browser_;  // weak

  // Used to build the bookmark submenu.
  scoped_ptr<BookmarkMenuBridge> bookmarkMenuBridge_;

  // Observer for page zoom level change notifications.
  scoped_ptr<WrenchMenuControllerInternal::ZoomLevelObserver>
      zoom_level_observer_;

  // Observer for the main window's ToolbarActionsBar changing size.
  scoped_ptr<WrenchMenuControllerInternal::ToolbarActionsBarObserverHelper>
      toolbar_actions_bar_observer_;

  // The controller for the toolbar actions overflow that is stored in the
  // wrench menu.
  // This will only be present if the extension action redesign switch is on.
  base::scoped_nsobject<BrowserActionsController> browserActionsController_;

  // The menu item containing the browser actions overflow container.
  NSMenuItem* browserActionsMenuItem_;
}

// Designated initializer.
- (id)initWithBrowser:(Browser*)browser;

// Used to dispatch commands from the Wrench menu. The custom items within the
// menu cannot be hooked up directly to First Responder because the window in
// which the controls reside is not the BrowserWindowController, but a
// NSCarbonMenuWindow; this screws up the typical |-commandDispatch:| system.
- (IBAction)dispatchWrenchMenuCommand:(id)sender;

// Returns the weak reference to the AppMenuModel.
- (AppMenuModel*)appMenuModel;

// Creates a RecentTabsMenuModelDelegate instance which will take care of
// updating the recent tabs submenu.
- (void)updateRecentTabsSubmenu;

// Updates the browser actions section of the menu.
- (void)updateBrowserActionsSubmenu;

// Retuns the weak reference to the BrowserActionsController.
- (BrowserActionsController*)browserActionsController;

@end

////////////////////////////////////////////////////////////////////////////////

// Shim view controller that merely unpacks objects from a NIB.
@interface WrenchMenuButtonViewController : NSViewController {
 @private
  WrenchMenuController* controller_;

  MenuTrackedRootView* editItem_;
  NSButton* editCut_;
  NSButton* editCopy_;
  NSButton* editPaste_;

  MenuTrackedRootView* zoomItem_;
  NSButton* zoomPlus_;
  NSButton* zoomDisplay_;
  NSButton* zoomMinus_;
  NSButton* zoomFullScreen_;

  MenuTrackedRootView* toolbarActionsOverflowItem_;
  BrowserActionsContainerView* overflowActionsContainerView_;
}

@property(assign, nonatomic) IBOutlet MenuTrackedRootView* editItem;
@property(assign, nonatomic) IBOutlet NSButton* editCut;
@property(assign, nonatomic) IBOutlet NSButton* editCopy;
@property(assign, nonatomic) IBOutlet NSButton* editPaste;
@property(assign, nonatomic) IBOutlet MenuTrackedRootView* zoomItem;
@property(assign, nonatomic) IBOutlet NSButton* zoomPlus;
@property(assign, nonatomic) IBOutlet NSButton* zoomDisplay;
@property(assign, nonatomic) IBOutlet NSButton* zoomMinus;
@property(assign, nonatomic) IBOutlet NSButton* zoomFullScreen;
@property(assign, nonatomic)
    IBOutlet MenuTrackedRootView* toolbarActionsOverflowItem;
@property(assign, nonatomic)
    IBOutlet BrowserActionsContainerView* overflowActionsContainerView;

- (id)initWithController:(WrenchMenuController*)controller;
- (IBAction)dispatchWrenchMenuCommand:(id)sender;

@end

#endif  // CHROME_BROWSER_UI_COCOA_WRENCH_MENU_WRENCH_MENU_CONTROLLER_H_
