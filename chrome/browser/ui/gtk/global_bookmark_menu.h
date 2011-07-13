// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GLOBAL_BOOKMARK_MENU_H_
#define CHROME_BROWSER_UI_GTK_GLOBAL_BOOKMARK_MENU_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/task.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/ui/gtk/global_menu_owner.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

class Browser;
class Profile;

typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GtkWidget GtkWidget;

// Manages the global bookmarks menu.
//
// There are a few subtleties here: we can't rely on accurate event
// dispositions being sent back, right click menus on menu items are placed
// relative to the main chrome window instead of the global menu bar, and we
// need to update the menu in the background (instead of building it on showing
// and not updating it if the model changes). I'm not even thinking about
// making these draggable since these items aren't displayed in our process.
class GlobalBookmarkMenu : public GlobalMenuOwner,
                           public NotificationObserver,
                           public BookmarkModelObserver {
 public:
  explicit GlobalBookmarkMenu(Browser* browser);
  virtual ~GlobalBookmarkMenu();

  // Takes the bookmark menu we need to modify based on bookmark state.
  virtual void Init(GtkWidget* bookmark_menu, GtkWidget* bookmark_menu_item);

 private:
  // Schedules the menu to be rebuilt. The mac version sets a boolean and
  // rebuilds the menu during their pre-show callback. We don't have anything
  // like that: by the time we get a "show" signal from GTK+, the menu has
  // already been displayed and it will take multiple dbus calls to add the
  // menu items.
  //
  // Since the bookmark model works by sending us BookmarkNodeEvent
  // notifications one by one, we use a timer to batch up calls.
  void RebuildMenuInFuture();

  // Rebuilds the menu now. Called on initial Load() and from
  // RebuildMenuInFuture().
  void RebuildMenu();

  // Adds |item| to |menu| and marks it as a dynamic item.
  void AddBookmarkMenuItem(GtkWidget* menu, GtkWidget* menu_item);

  // Adds an menu item representing |node| to |menu|.
  void AddNodeToMenu(const BookmarkNode* node, GtkWidget* menu);

  // This configures a GtkWidget with all the data from a BookmarkNode. This is
  // used to update existing menu items, as well as to configure newly created
  // ones, like in AddNodeToMenu().
  void ConfigureMenuItem(const BookmarkNode* node, GtkWidget* menu_item);

  // Returns the GtkMenuItem for |node|.
  GtkWidget* MenuItemForNode(const BookmarkNode* node);

  // Removes all bookmark entries from the bookmark menu in anticipation that
  // we're about to do a rebuild.
  void ClearBookmarkMenu();

  // Callback used in ClearBookmarkMenu().
  static void ClearBookmarkItemCallback(GtkWidget* menu_item,
                                        void* unused);

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // BookmarkModelObserver:
  virtual void Loaded(BookmarkModel* model, bool ids_reassigned) OVERRIDE;
  virtual void BookmarkModelBeingDeleted(BookmarkModel* model) OVERRIDE;
  virtual void BookmarkNodeMoved(BookmarkModel* model,
                                 const BookmarkNode* old_parent,
                                 int old_index,
                                 const BookmarkNode* new_parent,
                                 int new_index) OVERRIDE;
  virtual void BookmarkNodeAdded(BookmarkModel* model,
                                 const BookmarkNode* parent,
                                 int index) OVERRIDE;
  virtual void BookmarkNodeRemoved(BookmarkModel* model,
                                   const BookmarkNode* parent,
                                   int old_index,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeFaviconChanged(BookmarkModel* model,
                                          const BookmarkNode* node) OVERRIDE;
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node) OVERRIDE;

  CHROMEGTK_CALLBACK_0(GlobalBookmarkMenu, void, OnBookmarkItemActivated);

  Browser* browser_;
  Profile* profile_;

  NotificationRegistrar registrar_;

  GdkPixbuf* default_favicon_;
  GdkPixbuf* default_folder_;

  OwnedWidgetGtk bookmark_menu_;

  ScopedRunnableMethodFactory<GlobalBookmarkMenu> method_factory_;

  // In order to appropriately update items in the bookmark menu, without
  // forcing a rebuild, map the model's nodes to menu items.
  std::map<const BookmarkNode*, GtkWidget*> bookmark_nodes_;
};

#endif  // CHROME_BROWSER_UI_GTK_GLOBAL_BOOKMARK_MENU_H_
