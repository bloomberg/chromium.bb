// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_CONTEXT_MENU_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_CONTEXT_MENU_GTK_H_

#include <vector>

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/gtk/menu_gtk.h"

class Browser;
class PageNavigator;
class Profile;

// BookmarkContextMenu manages the context menu shown for the
// bookmark bar, items on the bookmark bar, submenus of the bookmark bar and
// the bookmark manager.
class BookmarkContextMenuGtk : public BookmarkModelObserver,
                               public MenuGtk::Delegate {
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

  class Delegate {
   public:
    // Called when one of the menu items is selected and executed.
    virtual void WillExecuteCommand() = 0;
  };

  // Creates the bookmark context menu.
  // |profile| is used for opening urls as well as enabling 'open incognito'.
  // |browser| is used to determine the PageNavigator and may be null.
  // |navigator| is used if |browser| is null, and is provided for testing.
  // |parent| is the parent for newly created nodes if |selection| is empty.
  // |selection| is the nodes the context menu operates on and may be empty.
  // |configuration| determines which items to show.
  BookmarkContextMenuGtk(gfx::NativeWindow hwnd,
                         Profile* profile,
                         Browser* browser,
                         PageNavigator* navigator,
                         const BookmarkNode* parent,
                         const std::vector<const BookmarkNode*>& selection,
                         ConfigurationType configuration,
                         Delegate* delegate);
  virtual ~BookmarkContextMenuGtk();

  // Pops up this menu. This call doesn't block.
  void PopupAsContext(guint32 event_time);

  // Returns the menu.
  GtkWidget* menu() const { return menu_->widget(); }

  // Should be called by the delegate when it is no longer valid.
  void DelegateDestroyed();

  // Menu::Delegate / MenuGtk::Delegate methods.
  virtual void ExecuteCommandById(int id);
  virtual bool IsItemChecked(int id) const;
  virtual bool IsCommandEnabled(int id) const;

 private:
  // BookmarkModelObserver method. Any change to the model results in closing
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

  // Builds the platform specific menu object.
  void CreateMenuObject();

  // Adds a IDS_* style command to the menu.
  void AppendItem(int id);
  // Adds a IDS_* style command to the menu with a different localized string.
  void AppendItem(int id, int localization_id);
  // Adds a separator to the menu.
  void AppendSeparator();
  // Adds a checkable item to the menu.
  void AppendCheckboxItem(int id);

  // Removes the observer from the model and NULLs out model_.
  BookmarkModel* RemoveModelObserver();

  // Returns true if selection_ has at least one bookmark of type url.
  bool HasURLs() const;

  // Returns the parent for newly created folders/bookmarks. If selection_
  // has one element and it is a folder, selection_[0] is returned, otherwise
  // parent_ is returned.
  const BookmarkNode* GetParentForNewNodes() const;

  gfx::NativeWindow wnd_;
  Profile* profile_;
  Browser* browser_;
  PageNavigator* navigator_;
  const BookmarkNode* parent_;
  std::vector<const BookmarkNode*> selection_;
  BookmarkModel* model_;
  ConfigurationType configuration_;
  Delegate* delegate_;
  scoped_ptr<MenuGtk> menu_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkContextMenuGtk);
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_CONTEXT_MENU_GTK_H_
