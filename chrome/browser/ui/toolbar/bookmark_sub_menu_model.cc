// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/bookmark_sub_menu_model.h"

#include "chrome/app/chrome_command_ids.h"
#include "grit/generated_resources.h"

// For views and cocoa, we have complex delegate systems to handle
// injecting the bookmarks to the bookmark submenu. This is done to support
// advanced interactions with the menu contents, like right click context menus.
// For the time being on GTK systems, we have a dedicated bookmark menu model in
// chrome/browser/ui/gtk/bookmarks/bookmark_sub_menu_model_gtk.cc instead.

// Note that although this file's header is included on GTK systems, this
// source file is not compiled there. (The header just includes the GTK one.)

BookmarkSubMenuModel::BookmarkSubMenuModel(
    ui::SimpleMenuModel::Delegate* delegate, Browser* browser)
    : SimpleMenuModel(delegate) {
  Build(browser);
}

BookmarkSubMenuModel::~BookmarkSubMenuModel() {}

void BookmarkSubMenuModel::Build(Browser* browser) {
  AddCheckItemWithStringId(IDC_SHOW_BOOKMARK_BAR, IDS_SHOW_BOOKMARK_BAR);
  AddItemWithStringId(IDC_SHOW_BOOKMARK_MANAGER, IDS_BOOKMARK_MANAGER);
#if !defined(OS_CHROMEOS)
  AddItemWithStringId(IDC_IMPORT_SETTINGS, IDS_IMPORT_SETTINGS_MENU_LABEL);
#endif
  AddSeparator(ui::NORMAL_SEPARATOR);
  AddItemWithStringId(IDC_BOOKMARK_PAGE, IDS_BOOKMARK_STAR);
  AddItemWithStringId(IDC_PIN_TO_START_SCREEN, IDS_PIN_TO_START_SCREEN);
#if defined(OS_MACOSX)
  AddSeparator(ui::NORMAL_SEPARATOR);
#endif
}
