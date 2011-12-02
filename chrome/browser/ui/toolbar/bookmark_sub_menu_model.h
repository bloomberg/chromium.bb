// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_
#pragma once

// For views and cocoa, we have complex delegate systems to handle
// injecting the bookmarks to the bookmark submenu. This is done to support
// advanced interactions with the menu contents, like right click context menus.
// For the time being on GTK systems, we have a dedicated bookmark menu model in
// chrome/browser/ui/gtk/bookmark_sub_menu_model_gtk.h instead.

#if defined(TOOLKIT_USES_GTK) && !defined(TOOLKIT_VIEWS)

#include "chrome/browser/ui/gtk/bookmarks/bookmark_sub_menu_model_gtk.h"

#else  // defined(TOOLKIT_USES_GTK) && !defined(TOOLKIT_VIEWS)

#include "ui/base/models/simple_menu_model.h"

class Browser;

class BookmarkSubMenuModel : public ui::SimpleMenuModel {
 public:
  BookmarkSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                       Browser* browser);
  virtual ~BookmarkSubMenuModel();

 private:
  void Build(Browser* browser);

  DISALLOW_COPY_AND_ASSIGN(BookmarkSubMenuModel);
};

#endif  // defined(TOOLKIT_USES_GTK) && !defined(TOOLKIT_VIEWS)

#endif  // CHROME_BROWSER_UI_TOOLBAR_BOOKMARK_SUB_MENU_MODEL_H_
