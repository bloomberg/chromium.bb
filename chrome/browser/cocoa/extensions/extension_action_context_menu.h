// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
#define CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"

#import <Cocoa/Cocoa.h>

class AsyncUninstaller;
class DevmodeObserver;
class Extension;
class ExtensionAction;
class Profile;

// A context menu used by the Browser and Page Action components that appears
// if a user right-clicks the view of the given extension.
@interface ExtensionActionContextMenu : NSMenu {
 @private
  // The extension that this menu belongs to. Weak.
  Extension* extension_;

  // The extension action this menu belongs to. Weak.
  ExtensionAction* action_;

  // The browser profile of the window that contains this extension. Weak.
  Profile* profile_;

  // The inspector menu item.  Need to keep this around to add and remove it.
  scoped_nsobject<NSMenuItem> inspectorItem_;

  scoped_ptr<DevmodeObserver> observer_;

  // Used to load the extension icon asynchronously on the I/O thread then show
  // the uninstall confirmation dialog.
  scoped_ptr<AsyncUninstaller> uninstaller_;
}

// Initializes and returns a context menu for the given extension and profile.
- (id)initWithExtension:(Extension*)extension
                profile:(Profile*)profile
        extensionAction:(ExtensionAction*)action;

// Show or hide the inspector menu item.
- (void)updateInspectorItem;
@end

typedef ExtensionActionContextMenu ExtensionActionContextMenuMac;

#endif  // CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
