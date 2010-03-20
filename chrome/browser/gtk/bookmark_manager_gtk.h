// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_MANAGER_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_MANAGER_GTK_H_

#include <gtk/gtk.h>
#include <vector>

#include "app/gtk_signal.h"
#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/bookmarks/bookmark_context_menu_controller.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/gtk/gtk_tree.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/shell_dialogs.h"
#include "gfx/rect.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class BookmarkModel;
class BookmarkTableModel;
class MenuGtk;
class Profile;

class BookmarkManagerGtk : public BookmarkModelObserver,
                           public ProfileSyncServiceObserver,
                           public gtk_tree::TableAdapter::Delegate,
                           public SelectFileDialog::Listener,
                           public BookmarkContextMenuControllerDelegate {
 public:
  virtual ~BookmarkManagerGtk();

  Profile* profile() { return profile_; }

  // Show the node in the tree.
  void SelectInTree(const BookmarkNode* node, bool expand);

  // Shows the bookmark manager. Only one bookmark manager exists.
  static void Show(Profile* profile);

  // BookmarkModelObserver implementation.
  // The implementations of these functions only affect the left side tree view
  // as changes to the right side are handled via TableModelObserver callbacks.
  virtual void Loaded(BookmarkModel* model);
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
                                   int old_index,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChanged(BookmarkModel* model,
                                   const BookmarkNode* node);
  virtual void BookmarkNodeChildrenReordered(BookmarkModel* model,
                                             const BookmarkNode* node);
  virtual void BookmarkNodeFavIconLoaded(BookmarkModel* model,
                                         const BookmarkNode* node);

  // gtk_tree::TableAdapter::Delegate implementation.
  virtual void SetColumnValues(int row, GtkTreeIter* iter);
  virtual void OnModelChanged();

  // SelectFileDialog::Listener implemenation.
  virtual void FileSelected(const FilePath& path,
                            int index, void* params);

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged();

  // BookmarkContextMenuController::Delegate implementation.
  virtual void CloseMenu();

 private:
  friend class BookmarkManagerTest;
  FRIEND_TEST(BookmarkManagerTest, Crash);

  explicit BookmarkManagerGtk(Profile* profile);

  void InitWidgets();
  GtkWidget* MakeLeftPane();
  GtkWidget* MakeRightPane();

  // Get the currently showing bookmark manager. Only used in testing.
  static BookmarkManagerGtk* GetCurrentManager();

  // Set |window_|'s initial size, using its previous size if that was saved.
  void SetInitialWindowSize();

  // Connects the ctrl-w accelerator to the window.
  void ConnectAccelerators();

  // If |left|, then make the organize menu refer to that which is selected in
  // the left pane, otherwise use the right pane selection.
  void ResetOrganizeMenu(bool left);

  // Pack the data from the bookmark model into the stores. This does not
  // create the stores, which is done in Make{Left,Right}Pane().
  // This one should only be called once (when the bookmark model is loaded).
  void BuildLeftStore();
  // This one clears the old right pane and refills it with the contents of
  // whatever folder is selected on the left. It can be called multiple times.
  void BuildRightStore();

  // Reset the TableModel backing the right pane.
  void ResetRightStoreModel();

  // Get the ID of the item at |iter|.
  int64 GetRowIDAt(GtkTreeModel* model, GtkTreeIter* iter);

  // Get the node from |model| at |iter|. If the item is not a node, return
  // NULL.
  const BookmarkNode* GetNodeAt(GtkTreeModel* model, GtkTreeIter* iter);

  // Get the node that is selected in the left tree view.
  const BookmarkNode* GetFolder();

  // Get the ID of the selected row.
  int GetSelectedRowID();

  // Get the nodes that are selected in the right tree view.
  std::vector<const BookmarkNode*> GetRightSelection();

  // Set the size of a column based on the user prefs, and also sets the sizing
  // properties of the column.
  void SizeColumn(GtkTreeViewColumn* column,
                  const wchar_t* prefname);

  // Calls SizeColumn() on each of the right tree view columns.
  void SizeColumns();

  // Save the column widths into the pref service. Column widths are stored
  // separately depending on whether the path column is showing.
  void SaveColumnConfiguration();

  // Flush the saved mousedown. Should only be called when |delaying_mousedown_|
  // is true.
  void SendDelayedMousedown();

  GtkTreeSelection* left_selection() {
    return gtk_tree_view_get_selection(GTK_TREE_VIEW(left_tree_view_));
  }

  GtkTreeSelection* right_selection() {
    return gtk_tree_view_get_selection(GTK_TREE_VIEW(right_tree_view_));
  }

  // Use these to temporarily disable updating the right store. You can stack
  // calls to Suppress(), but they should always be matched by an equal number
  // of calls to Allow().
  void SuppressUpdatesToRightStore();
  void AllowUpdatesToRightStore();

  // Tries to find the node with id |target_id|. If found, returns true and set
  // |iter| to point to the entry. If you pass a |iter| with stamp of 0, then it
  // will be treated as the first row of |model|.
  bool RecursiveFind(GtkTreeModel* model, GtkTreeIter* iter, int64 target_id);

  // Search helpers.
  void PerformSearch();

  CHROMEGTK_CALLBACK_0(BookmarkManagerGtk, void, OnSearchTextChanged);

  // TreeView signal handlers.
  static void OnLeftSelectionChanged(GtkTreeSelection* selection,
                                     BookmarkManagerGtk* bm);

  static void OnRightSelectionChanged(GtkTreeSelection* selection,
                                      BookmarkManagerGtk* bm);

  CHROMEGTK_CALLBACK_6(BookmarkManagerGtk, void, OnLeftTreeViewDragReceived,
                       GdkDragContext*, gint, gint, GtkSelectionData*,
                       guint, guint);
  CHROMEGTK_CALLBACK_4(BookmarkManagerGtk, gboolean, OnLeftTreeViewDragMotion,
                       GdkDragContext*, gint, gint, guint);

  CHROMEGTK_CALLBACK_2(BookmarkManagerGtk, void, OnLeftTreeViewRowCollapsed,
                       GtkTreeIter*, GtkTreePath*);
  CHROMEGTK_CALLBACK_4(BookmarkManagerGtk, void, OnRightTreeViewDragGet,
                       GdkDragContext*, GtkSelectionData*, guint, guint);

  CHROMEGTK_CALLBACK_6(BookmarkManagerGtk, void, OnRightTreeViewDragReceived,
                       GdkDragContext*, gint, gint, GtkSelectionData*,
                       guint, guint);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, void, OnRightTreeViewDragBegin,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_4(BookmarkManagerGtk, gboolean, OnRightTreeViewDragMotion,
                       GdkDragContext*, gint, gint, guint);

  CHROMEGTK_CALLBACK_2(BookmarkManagerGtk, void, OnRightTreeViewRowActivated,
                       GtkTreePath*, GtkTreeViewColumn*);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, void, OnRightTreeViewFocusIn,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, void, OnLeftTreeViewFocusIn,
                       GdkEventFocus*);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, gboolean, OnRightTreeViewButtonPress,
                       GdkEventButton*);

  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, gboolean, OnRightTreeViewMotion,
                       GdkEventMotion*);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, gboolean, OnTreeViewButtonPress,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, gboolean, OnTreeViewButtonRelease,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, gboolean, OnTreeViewKeyPress,
                       GdkEventKey*);

  // Callback from inline edits to folder names.  This changes the name in the
  // model.
  static void OnFolderNameEdited(GtkCellRendererText* render,
                                 gchar* path,
                                 gchar* new_folder_name,
                                 BookmarkManagerGtk* bm);

  // Tools menu item callbacks.
  CHROMEGTK_CALLBACK_0(BookmarkManagerGtk, void, OnImportItemActivated);
  CHROMEGTK_CALLBACK_0(BookmarkManagerGtk, void, OnExportItemActivated);

  // Sync status menu item callback.
  CHROMEGTK_CALLBACK_0(BookmarkManagerGtk, void, OnSyncStatusMenuActivated);

  // Window callbacks.
  CHROMEGTK_CALLBACK_0(BookmarkManagerGtk, gboolean, OnWindowDestroyed);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, gboolean, OnWindowStateChanged,
                       GdkEventWindowState*);
  CHROMEGTK_CALLBACK_1(BookmarkManagerGtk, gboolean, OnWindowConfigured,
                       GdkEventConfigure*);

  // Handles an accelerator being pressed.
  static gboolean OnGtkAccelerator(GtkAccelGroup* accel_group,
                                   GObject* acceleratable,
                                   guint keyval,
                                   GdkModifierType modifier,
                                   BookmarkManagerGtk* bookmark_manager);

  void UpdateSyncStatus();

  GtkWidget* window_;
  GtkWidget* search_entry_;
  Profile* profile_;
  BookmarkModel* model_;
  GtkWidget* paned_;
  GtkWidget* left_tree_view_;
  GtkWidget* right_tree_view_;

  enum {
    RIGHT_PANE_PIXBUF,
    RIGHT_PANE_TITLE,
    RIGHT_PANE_URL,
    RIGHT_PANE_PATH,
    RIGHT_PANE_ID,
    RIGHT_PANE_NUM
  };
  GtkTreeStore* left_store_;
  GtkListStore* right_store_;
  GtkTreeViewColumn* title_column_;
  GtkTreeViewColumn* url_column_;
  GtkTreeViewColumn* path_column_;
  scoped_ptr<BookmarkTableModel> right_tree_model_;
  scoped_ptr<gtk_tree::TableAdapter> right_tree_adapter_;

  // |window_|'s current position and size.
  gfx::Rect window_bounds_;

  // Flags describing |window_|'s current state.
  GdkWindowState window_state_;

  // The Organize menu item.
  GtkWidget* organize_;
  // The submenu the item pops up.
  // The controller.
  scoped_ptr<BookmarkContextMenuController> organize_menu_controller_;
  // The view.
  scoped_ptr<MenuGtk> organize_menu_;
  // Whether the menu refers to the left selection.
  bool organize_is_for_left_;

  // The context menu view and controller.
  scoped_ptr<BookmarkContextMenuController> context_menu_controller_;
  scoped_ptr<MenuGtk> context_menu_;

  // The sync status menu item that notifies the user about the current status
  // of bookmarks synchronization.
  GtkWidget* sync_status_menu_;

  // A pointer to the ProfileSyncService instance if one exists.
  ProfileSyncService* sync_service_;

  // True if the cached credentials have expired and we need to prompt the
  // user to re-enter their password.
  bool sync_relogin_required_;

  // Factory used for delaying search.
  ScopedRunnableMethodFactory<BookmarkManagerGtk> search_factory_;

  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // These variables are used for the workaround for http://crbug.com/15240.
  // The last mouse down we got. Only valid while |delaying_mousedown| is true.
  GdkEventButton mousedown_event_;
  bool delaying_mousedown_;
  // This is true while we are propagating a delayed mousedown. It is used to
  // tell the button press handler to ignore the event.
  bool sending_delayed_mousedown_;

  // This is used to avoid recursively calling our right click handler. It is
  // only true when a right click is already being handled.
  bool ignore_rightclicks_;

  GtkAccelGroup* accel_group_;
};

#endif  // CHROME_BROWSER_GTK_BOOKMARK_MANAGER_GTK_H_
