// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"

class AsyncUninstaller;
class Browser;
class ExtensionAction;

namespace extensions {
class Extension;
}

// Controller for extension context menu. This class builds the context menu
// and handles actions. This is mostly used when the user right clicks on
// page and browser actions in the toolbar.
@interface ExtensionActionContextMenuController : NSObject {
 @private
  // The extension that the menu belongs to. Weak.
  const extensions::Extension* extension_;

  // The extension action the menu belongs to. Weak.
  ExtensionAction* action_;

  // The browser that contains this extension. Weak.
  Browser* browser_;

  // Used to load the extension icon asynchronously on the I/O thread then show
  // the uninstall confirmation dialog.
  scoped_ptr<AsyncUninstaller> uninstaller_;
}

// Initializes and returns a context menu controller for the given extension and
// browser.
- (id)initWithExtension:(const extensions::Extension*)extension
                browser:(Browser*)browser
        extensionAction:(ExtensionAction*)action;

// Adds context menu items to the given menu.
- (void)populateMenu:(NSMenu*)menu;

@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
