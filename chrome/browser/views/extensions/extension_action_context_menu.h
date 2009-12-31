// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_

#include "views/controls/menu/menu_2.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_action_context_menu_model.h"

class Extension;

// Displays the context menu for extension action icons (browser/page actions).
class ExtensionActionContextMenu {
 public:
  ExtensionActionContextMenu();
  ~ExtensionActionContextMenu();

  // Display the context menu at a given point.
  void Run(Extension* extension, const gfx::Point& point);

 private:
  // The options menu.
  scoped_ptr<ExtensionActionContextMenuModel> context_menu_contents_;
  scoped_ptr<views::Menu2> context_menu_menu_;

  // The extension we are showing the menu for.
  Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionActionContextMenu);
};

#endif  // CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_ACTION_CONTEXT_MENU_H_
