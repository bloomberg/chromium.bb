// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INFOBAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INFOBAR_CONTROLLER_H_
#pragma once

#import "chrome/browser/ui/cocoa/infobars/infobar_controller.h"

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

@class ExtensionActionContextMenu;
class ExtensionInfoBarDelegate;
class InfobarBridge;
@class MenuButton;

@interface ExtensionInfoBarController : InfoBarController {
  // The native extension view retrieved from the extension host. Weak.
  NSView* extensionView_;

  // The window containing this InfoBar. Weak.
  NSWindow* window_;

  // The InfoBar's button with the Extension's icon that launches the context
  // menu.
  scoped_nsobject<MenuButton> dropdownButton_;

  // The context menu that pops up when the left button is clicked.
  scoped_nsobject<ExtensionActionContextMenu> contextMenu_;

  // Helper class to bridge C++ and ObjC functionality together for the infobar.
  scoped_ptr<InfobarBridge> bridge_;
}

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INFOBAR_CONTROLLER_H_
