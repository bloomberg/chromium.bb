// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"

class AsyncUninstaller;
class ExtensionAction;
class Profile;

namespace extensions {
class Extension;
}

namespace extension_action_context_menu {

}  // namespace extension_action_context_menu

// A context menu used by any extension UI components that require it.
@interface ExtensionActionContextMenu : NSMenu {
 @private
  // The extension that this menu belongs to. Weak.
  const extensions::Extension* extension_;

  // The extension action this menu belongs to. Weak.
  ExtensionAction* action_;

  // The browser profile of the window that contains this extension. Weak.
  Profile* profile_;

  // Used to load the extension icon asynchronously on the I/O thread then show
  // the uninstall confirmation dialog.
  scoped_ptr<AsyncUninstaller> uninstaller_;
}

// Initializes and returns a context menu for the given extension and profile.
- (id)initWithExtension:(const extensions::Extension*)extension
                profile:(Profile*)profile
        extensionAction:(ExtensionAction*)action;

@end

typedef ExtensionActionContextMenu ExtensionActionContextMenuMac;

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
