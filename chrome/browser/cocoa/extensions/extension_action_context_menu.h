// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
#define CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_

#include "base/ref_counted.h"

#import <Cocoa/Cocoa.h>

class Extension;
class AsyncUninstaller;

// A context menu used by the Browser and Page Action components that appears
// if a user right-clicks the view of the given extension.
@interface ExtensionActionContextMenu : NSMenu {
  // The extension that this menu belongs to. Weak.
  Extension* extension_;

  // Used to load the extension icon asynchronously on the I/O thread then show
  // the uninstall confirmation dialog.
  scoped_refptr<AsyncUninstaller> uninstaller_;
}

// Initializes and returns a context menu for the given extension.
- (id)initWithExtension:(Extension*)extension;

@end

typedef ExtensionActionContextMenu ExtensionActionContextMenuMac;

#endif  // CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
