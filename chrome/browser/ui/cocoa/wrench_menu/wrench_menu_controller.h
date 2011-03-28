// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WRENCH_MENU_WRENCH_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_WRENCH_MENU_WRENCH_MENU_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/menu_controller.h"

@class MenuTrackedRootView;
@class ToolbarController;
class WrenchMenuModel;

namespace WrenchMenuControllerInternal {
class ZoomLevelObserver;
}  // namespace WrenchMenuControllerInternal

// The Wrench menu has a creative layout, with buttons in menu items. There is
// a cross-platform model for this special menu, but on the Mac it's easier to
// get spacing and alignment precisely right using a NIB. To do that, we
// subclass the generic MenuController implementation and special-case the two
// items that require specific layout and load them from the NIB.
//
// This object is instantiated in Toolbar.xib and is configured by the
// ToolbarController.
@interface WrenchMenuController : MenuController<NSMenuDelegate> {
  IBOutlet MenuTrackedRootView* editItem_;
  IBOutlet NSButton* editCut_;
  IBOutlet NSButton* editCopy_;
  IBOutlet NSButton* editPaste_;

  IBOutlet MenuTrackedRootView* zoomItem_;
  IBOutlet NSButton* zoomPlus_;
  IBOutlet NSButton* zoomDisplay_;
  IBOutlet NSButton* zoomMinus_;
  IBOutlet NSButton* zoomFullScreen_;

  scoped_ptr<WrenchMenuControllerInternal::ZoomLevelObserver> observer_;
}

// Designated initializer; called within the NIB.
- (id)init;

// Used to dispatch commands from the Wrench menu. The custom items within the
// menu cannot be hooked up directly to First Responder because the window in
// which the controls reside is not the BrowserWindowController, but a
// NSCarbonMenuWindow; this screws up the typical |-commandDispatch:| system.
- (IBAction)dispatchWrenchMenuCommand:(id)sender;

// Returns the weak reference to the WrenchMenuModel.
- (WrenchMenuModel*)wrenchMenuModel;

@end

////////////////////////////////////////////////////////////////////////////////

@interface WrenchMenuController (UnitTesting)
// |-dispatchWrenchMenuCommand:| calls this after it has determined the tag of
// the sender. The default implementation executes the command on the outermost
// run loop using |-performSelector...withDelay:|. This is not desirable in
// unit tests because it's hard to test around run loops in a deterministic
// manner. To avoid those headaches, tests should provide an alternative
// implementation.
- (void)dispatchCommandInternal:(NSInteger)tag;
@end

#endif  // CHROME_BROWSER_UI_COCOA_WRENCH_MENU_WRENCH_MENU_CONTROLLER_H_
