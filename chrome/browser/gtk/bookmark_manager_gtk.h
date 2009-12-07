// Copyright (c) 2009 The Chromium Authors. All rights reserved.TIT
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BOOKMARK_MANAGER_GTK_H_
#define CHROME_BROWSER_GTK_BOOKMARK_MANAGER_GTK_H_

#include <gtk/gtk.h>
#include <vector>

#include "base/basictypes.h"
#include "base/gfx/rect.h"
#include "base/ref_counted.h"
#include "base/task.h"
#include "chrome/browser/bookmarks/bookmark_model_observer.h"
#include "chrome/browser/gtk/bookmark_context_menu_gtk.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/common/gtk_tree.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

class BookmarkModel;
class BookmarkTableModel;
class Profile;

class BookmarkManagerGtk : public BookmarkModelObserver,
                           public ProfileSyncServiceObserver,
                           public gtk_tree::TableAdapter::Delegate,
                           public SelectFileDialog::Listener {
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

  void OnSearchTextChanged();

  static void OnSearchTextChangedThunk(GtkWidget* search_entry,
                                       BookmarkManagerGtk* bookmark_manager) {
    bookmark_manager->OnSearchTextChanged();
  }

  // TreeView signal handlers.
  static void OnLeftSelectionChanged(GtkTreeSelection* selection,
                                     BookmarkManagerGtk* bm);

  static void OnRightSelectionChanged(GtkTreeSelection* selection,
                                      BookmarkManagerGtk* bm);

  static void OnLeftTreeViewDragReceived(GtkWidget* tree_view,
                                         GdkDragContext* context,
                                         gint x,
                                         gint y,
                                         GtkSelectionData* selection_data,
                                         guint target_type,
                                         guint time,
                                         BookmarkManagerGtk* bm);

  static gboolean OnLeftTreeViewDragMotion(GtkWidget* tree_view,
                                           GdkDragContext* context,
                                           gint x,
                                           gint y,
                                           guint time,
                                           BookmarkManagerGtk* bm);

  static void OnLeftTreeViewRowCollapsed(GtkTreeView* tree_view,
                                         GtkTreeIter* iter,
                                         GtkTreePath* path,
                                         BookmarkManagerGtk* bm);

  static void OnRightTreeViewDragGet(GtkWidget* tree_view,
                                     GdkDragContext* context,
                                     GtkSelectionData* selection_data,
                                     guint target_type,
                                     guint time,
                                     BookmarkManagerGtk* bm);

  static void OnRightTreeViewDragReceived(GtkWidget* tree_view,
                                          GdkDragContext* context,
                                          gint x,
                                          gint y,
                                          GtkSelectionData* selection_data,
                                          guint target_type,
                                          guint time,
                                          BookmarkManagerGtk* bm);

  static void OnRightTreeViewDragBegin(GtkWidget* tree_view,
                                       GdkDragContext* drag_context,
                                       BookmarkManagerGtk* bm);

  static gboolean OnRightTreeViewDragMotion(GtkWidget* tree_view,
                                            GdkDragContext* context,
                                            gint x,
                                            gint y,
                                            guint time,
                                            BookmarkManagerGtk* bm);

  static void OnRightTreeViewRowActivated(GtkTreeView* tree_view,
                                          GtkTreePath* path,
                                          GtkTreeViewColumn* column,
                                          BookmarkManagerGtk* bm);

  static void OnRightTreeViewFocusIn(GtkTreeView* tree_view,
                                     GdkEventFocus* event,
                                     BookmarkManagerGtk* bm);

  static void OnLeftTreeViewFocusIn(GtkTreeView* tree_view,
                                    GdkEventFocus* event,
                                    BookmarkManagerGtk* bm);

  static gboolean OnRightTreeViewButtonPress(GtkWidget* tree_view,
                                             GdkEventButton* event,
                                             BookmarkManagerGtk* bm);

  static gboolean OnRightTreeViewMotion(GtkWidget* tree_view,
                                        GdkEventMotion* event,
                                        BookmarkManagerGtk* bm);

  static gboolean OnTreeViewButtonPress(GtkWidget* tree_view,
                                        GdkEventButton* button,
                                        BookmarkManagerGtk* bm);

  static gboolean OnTreeViewButtonRelease(GtkWidget* tree_view,
                                          GdkEventButton* button,
                                          BookmarkManagerGtk* bm);

  static gboolean OnTreeViewKeyPress(GtkWidget* tree_view,
                                     GdkEventKey* key,
                                     BookmarkManagerGtk* bm);

  // Callback from inline edits to folder names.  This changes the name in the
  // model.
  static void OnFolderNameEdited(GtkCellRendererText* render,
                                 gchar* path,
                                 gchar* new_folder_name,
                                 BookmarkManagerGtk* bm);

  // Tools menu item callbacks.
  static void OnImportItemActivated(GtkMenuItem* menuitem,
                                    BookmarkManagerGtk* bm);
  static void OnExportItemActivated(GtkMenuItem* menuitem,
                                    BookmarkManagerGtk* bm);

  // Sync status menu item callback.
  static void OnSyncStatusMenuActivated(GtkMenuItem* menu_item,
                                        BookmarkManagerGtk* bm);

  // Window callbacks.
  static gboolean OnWindowDestroyedThunk(GtkWidget* window, gpointer self) {
    return reinterpret_cast<BookmarkManagerGtk*>(self)->
        OnWindowDestroyed(window);
  }
  gboolean OnWindowDestroyed(GtkWidget* window);

  static gboolean OnWindowStateChangedThunk(GtkWidget* window,
                                            GdkEventWindowState* event,
                                            gpointer self) {
    return reinterpret_cast<BookmarkManagerGtk*>(self)->
        OnWindowStateChanged(window, event);
  }
  gboolean OnWindowStateChanged(GtkWidget* window, GdkEventWindowState* event);

  static gboolean OnWindowConfiguredThunk(GtkWidget* window,
                                          GdkEventConfigure* event,
                                          gpointer self) {
    return reinterpret_cast<BookmarkManagerGtk*>(self)->
        OnWindowConfigured(window, event);
  }
  gboolean OnWindowConfigured(GtkWidget* window, GdkEventConfigure* event);

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
  scoped_ptr<BookmarkContextMenuGtk> organize_menu_;
  // Whether the menu refers to the left selection.
  bool organize_is_for_left_;

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
