// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_INFOBARS_EXTENSION_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_INFOBARS_EXTENSION_INFOBAR_CONTROLLER_H_

#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

@class ExtensionActionContextMenuController;
class InfobarBridge;
@class MenuButton;

@interface ExtensionInfoBarController : InfoBarController<NSMenuDelegate> {
  // The native extension view retrieved from the extension host. Weak.
  NSView* extensionView_;

  // The InfoBar's button with the Extension's icon that launches the context
  // menu.
  base::scoped_nsobject<MenuButton> dropdownButton_;

  // Controller for the context menu when the left button is clicked.
  base::scoped_nsobject<
      ExtensionActionContextMenuController> contextMenuController_;

  // Helper class to bridge C++ and ObjC functionality together for the infobar.
  scoped_ptr<InfobarBridge> bridge_;
}

@end

#endif  // CHROME_BROWSER_UI_COCOA_INFOBARS_EXTENSION_INFOBAR_CONTROLLER_H_
