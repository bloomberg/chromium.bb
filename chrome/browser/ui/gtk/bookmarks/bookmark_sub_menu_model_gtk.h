// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_SUB_MENU_MODEL_GTK_H_
#define CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_SUB_MENU_MODEL_GTK_H_

#include <vector>

#include "chrome/browser/bookmarks/base_bookmark_model_observer.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/window_open_disposition.h"

class Browser;
class BookmarkModel;
class BookmarkNode;
class MenuGtk;  // See below for why we need this.

namespace content {
class PageNavigator;
}

// BookmarkNodeMenuModel builds a SimpleMenuModel on demand when the menu is
// shown, and automatically destroys child models when the menu is closed.
class BookmarkNodeMenuModel : public ui::SimpleMenuModel {
 public:
  BookmarkNodeMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                        BookmarkModel* model,
                        const BookmarkNode* node,
                        content::PageNavigator* page_navigator);
  virtual ~BookmarkNodeMenuModel();

  // From SimpleMenuModel. Takes care of deleting submenus.
  // Note that this is not virtual. That's OK for our use.
  void Clear();

  // From MenuModel via SimpleMenuModel.
  virtual void MenuWillShow() OVERRIDE;
  virtual void MenuClosed() OVERRIDE;
  virtual void ActivatedAt(int index) OVERRIDE;
  virtual void ActivatedAt(int index, int event_flags) OVERRIDE;

 protected:
  // Adds all bookmark items to the model. Does not clear the model first.
  void PopulateMenu();

  // Add a submenu for the given bookmark folder node.
  void AddSubMenuForNode(const BookmarkNode* node);

  BookmarkModel* model() const { return model_; }
  void set_model(BookmarkModel* model) { model_ = model; }

  const BookmarkNode* node() const { return node_; }
  void set_node(const BookmarkNode* node) { node_ = node; }

 private:
  // Uses the page navigator to open the bookmark at the given index.
  void NavigateToMenuItem(int index, WindowOpenDisposition disposition);

  // The bookmark model whose bookmarks we will show. Note that in the top-level
  // bookmark menu, this may be null. (It is set only when the menu is shown.)
  BookmarkModel* model_;

  // The bookmark node for the folder that this model will show. Note that in
  // the top-level bookmark menu, this may be null, as above.
  const BookmarkNode* node_;

  // The page navigator used to open bookmarks in ActivatedAt().
  content::PageNavigator* page_navigator_;

  // A list of the submenus we own and will need to delete.
  std::vector<BookmarkNodeMenuModel*> submenus_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkNodeMenuModel);
};

// This is the top-level bookmark menu model. It handles prepending a few fixed
// items before the bookmarks. Child menus are all plain BookmarkNodeMenuModels.
// This class also handles watching the bookmark model and forcing the menu to
// close if it changes while the menu is open.
class BookmarkSubMenuModel : public BookmarkNodeMenuModel,
                             public BaseBookmarkModelObserver {
 public:
  BookmarkSubMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                       Browser* browser);

  virtual ~BookmarkSubMenuModel();

  // See below; this is used to allow closing the menu when bookmarks change.
  void SetMenuGtk(MenuGtk* menu) { menu_ = menu; }

  // From BookmarkModelObserver, BaseBookmarkModelObserver.
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelChanged() OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;

  // From MenuModel via BookmarkNodeMenuModel, SimpleMenuModel.
  virtual void MenuWillShow() OVERRIDE;
  virtual void MenuClosed() OVERRIDE;
  virtual void ActivatedAt(int index) OVERRIDE;
  virtual void ActivatedAt(int index, int event_flags) OVERRIDE;
  virtual bool IsEnabledAt(int index) const OVERRIDE;
  virtual bool IsVisibleAt(int index) const OVERRIDE;

  // Returns true if the command id is for a bookmark item.
  static bool IsBookmarkItemCommandId(int command_id);

 private:
  Browser* browser_;

  // The number of fixed items shown before the bookmarks.
  int fixed_items_;
  // The index of the first non-bookmark item after the bookmarks.
  int bookmark_end_;

  // We need to be able to call Cancel() on the wrench menu when bookmarks
  // change. This is a bit of an abstraction violation but it could be converted
  // to an interface with just a Cancel() method if necessary.
  MenuGtk* menu_;

  // We keep track of whether the bookmark submenu is currently showing. If it's
  // not showing, then we don't need to forcibly close the entire wrench menu
  // when the bookmark model loads.
  bool menu_showing_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkSubMenuModel);
};

#endif  // CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_SUB_MENU_MODEL_GTK_H_
