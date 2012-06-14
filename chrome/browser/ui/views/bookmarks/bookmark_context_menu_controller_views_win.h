// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_VIEWS_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_VIEWS_WIN_H_
#pragma once

#include <vector>

#include "chrome/browser/ui/views/bookmarks/bookmark_context_menu_controller_views.h"

class BookmarkContextMenuControllerViewsWin
    : public BookmarkContextMenuControllerViews {
 public:
  BookmarkContextMenuControllerViewsWin(
      views::Widget* parent_widget,
      BookmarkContextMenuControllerViewsDelegate* delegate,
      Profile* profile,
      content::PageNavigator* navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection);
  virtual ~BookmarkContextMenuControllerViewsWin();

  // BookmarkContextMenuControllerViews overrides
  virtual void ExecuteCommand(int id) OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuControllerViewsWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_VIEWS_WIN_H_
