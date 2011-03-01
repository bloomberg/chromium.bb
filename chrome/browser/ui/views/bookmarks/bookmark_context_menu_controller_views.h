// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_VIEWS_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class PageNavigator;
class Profile;

// An interface implemented by an object that performs actions on the actual
// menu for the controller.
class BookmarkContextMenuControllerViewsDelegate {
 public:
  virtual ~BookmarkContextMenuControllerViewsDelegate() {}

  // Closes the bookmark context menu.
  virtual void CloseMenu() = 0;

  // Methods that add items to the underlying menu.
  virtual void AddItemWithStringId(int command_id, int string_id) = 0;
  virtual void AddSeparator() = 0;
  virtual void AddCheckboxItem(int command_id, int string_id) = 0;

  // Sent before bookmarks are removed.
  virtual void WillRemoveBookmarks(
      const std::vector<const BookmarkNode*>& bookmarks) {}

  // Sent after bookmarks have been removed.
  virtual void DidRemoveBookmarks() {}
};

// BookmarkContextMenuControllerViews creates and manages state for the context
// menu shown for any bookmark item.
class BookmarkContextMenuControllerViews : public BaseBookmarkModelObserver {
 public:
  // Creates the bookmark context menu.
  // |profile| is used for opening urls as well as enabling 'open incognito'.
  // |browser| is used to determine the PageNavigator and may be null.
  // |navigator| is used if |browser| is null, and is provided for testing.
  // |parent| is the parent for newly created nodes if |selection| is empty.
  // |selection| is the nodes the context menu operates on and may be empty.
  BookmarkContextMenuControllerViews(
      gfx::NativeWindow parent_window,
      BookmarkContextMenuControllerViewsDelegate* delegate,
      Profile* profile,
      PageNavigator* navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection);
  virtual ~BookmarkContextMenuControllerViews();

  void BuildMenu();

  void ExecuteCommand(int id);
  bool IsItemChecked(int id) const;
  bool IsCommandEnabled(int id) const;

  // Accessors:
  Profile* profile() const { return profile_; }
  PageNavigator* navigator() const { return navigator_; }

 private:
  // Overridden from BaseBookmarkModelObserver:
  // Any change to the model results in closing the menu.
  virtual void BookmarkModelChanged();

  // Removes the observer from the model and NULLs out model_.
  BookmarkModel* RemoveModelObserver();

  // Returns true if selection_ has at least one bookmark of type url.
  bool HasURLs() const;

  gfx::NativeWindow parent_window_;
  BookmarkContextMenuControllerViewsDelegate* delegate_;
  Profile* profile_;
  PageNavigator* navigator_;
  const BookmarkNode* parent_;
  std::vector<const BookmarkNode*> selection_;
  BookmarkModel* model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuControllerViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_VIEWS_H_
