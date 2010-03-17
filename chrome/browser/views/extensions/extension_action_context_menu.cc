// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_action_context_menu.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"

ExtensionActionContextMenu::ExtensionActionContextMenu()
  : extension_(NULL) {
}

ExtensionActionContextMenu::~ExtensionActionContextMenu() {
}

void ExtensionActionContextMenu::Run(Extension* extension,
    ExtensionAction* extension_action,
    ExtensionActionContextMenuModel::MenuDelegate* delegate,
    PrefService* prefs,
    const gfx::Point& point) {
  extension_ = extension;

  context_menu_contents_.reset(
      new ExtensionActionContextMenuModel(extension, extension_action, prefs,
                                          delegate));
  context_menu_menu_.reset(new views::Menu2(context_menu_contents_.get()));
  // This call blocks until the menu is dismissed or something is selected.
  context_menu_menu_->RunContextMenuAt(point);
}

void ExtensionActionContextMenu::Cancel() {
  if (context_menu_menu_.get())
    context_menu_menu_->CancelMenu();
}
