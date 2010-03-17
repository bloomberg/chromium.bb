// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_

#include <vector>

#include "app/menus/simple_menu_model.h"
#include "base/basictypes.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "gfx/native_widget_types.h"

class Browser;
class PageNavigator;
class Profile;

// An interface implemented by an object that performs actions on the actual
// menu for the controller.
class BookmarkContextMenuControllerDelegate {
 public:
  virtual ~BookmarkContextMenuControllerDelegate() {}

  // Closes the bookmark context menu.
  virtual void CloseMenu() = 0;

  // Sent before any command from the menu is executed.
  virtual void WillExecuteCommand() {}

  // Sent after any command from the menu is executed.
  virtual void DidExecuteCommand() {}
};

// BookmarkContextMenuController creates and manages state for the context menu
// shown for any bookmark item.
class BookmarkContextMenuController : public BookmarkModelObserver,
                                      public menus::SimpleMenuModel::Delegate {
 public:
  // Used to configure what the context menu shows.
  enum ConfigurationType {
    BOOKMARK_BAR,
    BOOKMARK_MANAGER_TABLE,
    // Used when the source is the table in the bookmark manager and the table
    // is showing recently bookmarked or searched.
    BOOKMARK_MANAGER_TABLE_OTHER,
    BOOKMARK_MANAGER_TREE,
    BOOKMARK_MANAGER_ORGANIZE_MENU,
    // Used when the source is the bookmark manager and the table is showing
    // recently bookmarked or searched.
    BOOKMARK_MANAGER_ORGANIZE_MENU_OTHER
  };

  // Creates the bookmark context menu.
  // |profile| is used for opening urls as well as enabling 'open incognito'.
  // |browser| is used to determine the PageNavigator and may be null.
  // |navigator| is used if |browser| is null, and is provided for testing.
  // |parent| is the parent for newly created nodes if |selection| is empty.
  // |selection| is the nodes the context menu operates on and may be empty.
  // |configuration| determines which items to show.
  BookmarkContextMenuController(
      gfx::NativeWindow parent_window,
      BookmarkContextMenuControllerDelegate* delegate,
      Profile* profile,
      PageNavigator* navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection,
      ConfigurationType configuration);
  virtual ~BookmarkContextMenuController();

  void BuildMenu();

  menus::SimpleMenuModel* menu_model() {
    return menu_model_.get();
  }


  // menus::SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      menus::Accelerator* accelerator) {
    return false;
  }
  virtual void ExecuteCommand(int command_id);

  // Accessors:
  Profile* profile() const { return profile_; }
  PageNavigator* navigator() const { return navigator_; }

 private:
  // Adds a IDS_* style command to the menu.
  void AddItem(int id);
  // Adds a IDS_* style command to the menu with a different localized string.
  void AddItem(int id, int localization_id);
  // Adds a separator to the menu.
  void AddSeparator();
  // Adds a checkable item to the menu.
  void AddCheckboxItem(int id);

  // BookmarkModelObserver methods. Any change to the model results in closing
  // the menu.
  virtual void Loaded(BookmarkModel* model) {}
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model);
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index);
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index);
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int index,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node) {}
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node);

  // Invoked from the various bookmark model observer methods. Closes the menu.
  void ModelChanged();

  // Returns true if selection_ has at least one bookmark of type url.
  bool HasURLs() const;

  // Returns the parent for newly created folders/bookmarks. If selection_
  // has one element and it is a folder, selection_[0] is returned, otherwise
  // parent_ is returned.
  const BookmarkNode* GetParentForNewNodes() const;

  gfx::NativeWindow parent_window_;
  BookmarkContextMenuControllerDelegate* delegate_;
  Profile* profile_;
  PageNavigator* navigator_;
  const BookmarkNode* parent_;
  std::vector<const BookmarkNode*> selection_;
  ConfigurationType configuration_;
  BookmarkModel* model_;
  scoped_ptr<menus::SimpleMenuModel> menu_model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuController);
};

#endif  // CHROME_BROWSER_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_
