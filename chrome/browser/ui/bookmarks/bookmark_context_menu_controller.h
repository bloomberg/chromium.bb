// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_
#define CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class Profile;

namespace content {
class PageNavigator;
}

// An interface implemented by an object that performs actions on the actual
// menu for the controller.
class BookmarkContextMenuControllerDelegate {
 public:
  virtual ~BookmarkContextMenuControllerDelegate() {}

  // Closes the bookmark context menu.
  virtual void CloseMenu() = 0;

  // Sent before any command from the menu is executed.
  virtual void WillExecuteCommand(
      int command_id,
      const std::vector<const BookmarkNode*>& bookmarks) {}

  // Sent after any command from the menu is executed.
  virtual void DidExecuteCommand(int command_id) {}
};

// BookmarkContextMenuController creates and manages state for the context menu
// shown for any bookmark item.
class BookmarkContextMenuController : public BaseBookmarkModelObserver,
                                      public ui::SimpleMenuModel::Delegate {
 public:
  // Creates the bookmark context menu.
  // |browser| is used to open the bookmark manager and is NULL in tests.
  // |profile| is used for opening urls as well as enabling 'open incognito'.
  // |navigator| is used if |browser| is null, and is provided for testing.
  // |parent| is the parent for newly created nodes if |selection| is empty.
  // |selection| is the nodes the context menu operates on and may be empty.
  BookmarkContextMenuController(
      gfx::NativeWindow parent_window,
      BookmarkContextMenuControllerDelegate* delegate,
      Browser* browser,
      Profile* profile,
      content::PageNavigator* navigator,
      const BookmarkNode* parent,
      const std::vector<const BookmarkNode*>& selection);
  virtual ~BookmarkContextMenuController();

  ui::SimpleMenuModel* menu_model() { return menu_model_.get(); }

  // ui::SimpleMenuModel::Delegate implementation:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool IsCommandIdVisible(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual base::string16 GetLabelForCommandId(int command_id) const OVERRIDE;

  void set_navigator(content::PageNavigator* navigator) {
    navigator_ = navigator;
  }

 private:
  void BuildMenu();

  // Adds a IDC_* style command to the menu with a localized string.
  void AddItem(int id, int localization_id);
  // Adds a separator to the menu.
  void AddSeparator();
  // Adds a checkable item to the menu.
  void AddCheckboxItem(int id, int localization_id);

  // Overridden from BaseBookmarkModelObserver:
  // Any change to the model results in closing the menu.
  virtual void BookmarkModelChanged() OVERRIDE;

  gfx::NativeWindow parent_window_;
  BookmarkContextMenuControllerDelegate* delegate_;
  Browser* browser_;
  Profile* profile_;
  content::PageNavigator* navigator_;
  const BookmarkNode* parent_;
  std::vector<const BookmarkNode*> selection_;
  BookmarkModel* model_;
  scoped_ptr<ui::SimpleMenuModel> menu_model_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuController);
};

#endif  // CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_CONTEXT_MENU_CONTROLLER_H_
