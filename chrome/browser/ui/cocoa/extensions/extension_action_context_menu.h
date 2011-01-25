// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/scoped_ptr.h"
#include "base/scoped_nsobject.h"

class AsyncUninstaller;
class DevmodeObserver;
class Extension;
class ExtensionAction;
class NotificationRegistrar;
class Profile;

namespace extension_action_context_menu {

class DevmodeObserver;
class ProfileObserverBridge;

}  // namespace extension_action_context_menu

// A context menu used by any extension UI components that require it.
@interface ExtensionActionContextMenu : NSMenu {
 @private
  // The extension that this menu belongs to. Weak.
  const Extension* extension_;

  // The extension action this menu belongs to. Weak.
  ExtensionAction* action_;

  // The browser profile of the window that contains this extension. Weak.
  Profile* profile_;

  // The inspector menu item.  Need to keep this around to add and remove it.
  scoped_nsobject<NSMenuItem> inspectorItem_;

  // The observer used to listen for pref changed notifications.
  scoped_ptr<extension_action_context_menu::DevmodeObserver> observer_;

  // The observer used to reset |observer_| when the profile is destroyed.
  scoped_ptr<extension_action_context_menu::ProfileObserverBridge>
      profile_observer_;

  // Used to load the extension icon asynchronously on the I/O thread then show
  // the uninstall confirmation dialog.
  scoped_ptr<AsyncUninstaller> uninstaller_;
}

// Initializes and returns a context menu for the given extension and profile.
- (id)initWithExtension:(const Extension*)extension
                profile:(Profile*)profile
        extensionAction:(ExtensionAction*)action;

// Show or hide the inspector menu item.
- (void)updateInspectorItem;

// Notifies the ExtensionActionContextMenu that the profile is is being
// destroyed.
- (void)invalidateProfile;

@end

typedef ExtensionActionContextMenu ExtensionActionContextMenuMac;

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
