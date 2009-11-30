// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_
#define CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_

#include <gtk/gtk.h>

#include <string>

#include "chrome/browser/task_manager.h"
#include "grit/generated_resources.h"

class TaskManagerGtk : public TaskManagerModelObserver {
 public:
  TaskManagerGtk();
  virtual ~TaskManagerGtk();

  // TaskManagerModelObserver
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

  // Creates the task manager if it doesn't exist; otherwise, it activates the
  // existing task manager window.
  static void Show();

 private:
  class ContextMenuController;
  friend class ContextMenuController;

  // Initializes the task manager dialog.
  void Init();

  // Set |dialog_|'s initial size, using its previous size if that was saved.
  void SetInitialDialogSize();

  // Connects the ctrl-w accelerator to the dialog.
  void ConnectAccelerators();

  // Sets up the treeview widget.
  void CreateTaskManagerTreeview();

  // Returns the model data for a given |row| and |col_id|.
  std::string GetModelText(int row, int col_id);

  // Retrieves the resource icon from the model for |row|.
  GdkPixbuf* GetModelIcon(int row);

  // Sets the treeview row data.  |row| is an index into the model and |iter|
  // is the current position in the treeview.
  void SetRowDataFromModel(int row, GtkTreeIter* iter);

  // Queries the treeview for the selected rows, and kills those processes.
  void KillSelectedProcesses();

  // Opens the context menu used to select the task manager columns.
  void ShowContextMenu();

  // Activates the tab associated with the focused row.
  void ActivateFocusedTab();

  // Opens about:memory in a new foreground tab.
  void OnLinkActivated();

  // Compare implementation used for sorting columns.
  gint CompareImpl(GtkTreeModel* tree_model, GtkTreeIter* a,
                   GtkTreeIter* b, int id);

  // response signal handler that notifies us of dialog destruction.
  static void OnDestroy(GtkDialog* dialog, TaskManagerGtk* task_manager);

  // response signal handler that notifies us of dialog responses.
  static void OnResponse(GtkDialog* dialog, gint response_id,
                         TaskManagerGtk* task_manager);

  // realize signal handler to set the page column's initial size.
  static void OnTreeViewRealize(GtkTreeView* treeview,
                                TaskManagerGtk* task_manager);

  // changed signal handler that is sent when the treeview selection changes.
  static void OnSelectionChanged(GtkTreeSelection* selection,
                                 TaskManagerGtk* task_manager);

  // button-press-event handler that activates a process on double-click.
  static gboolean OnButtonPressEvent(GtkWidget* widget, GdkEventButton* event,
                                     TaskManagerGtk* task_manager);

  // button-release-event handler that opens the right-click context menu.
  static gboolean OnButtonReleaseEvent(GtkWidget* widget, GdkEventButton* event,
                                       TaskManagerGtk* task_manager);

  // Handles an accelerator being pressed.
  static gboolean OnGtkAccelerator(GtkAccelGroup* accel_group,
                                   GObject* acceleratable,
                                   guint keyval,
                                   GdkModifierType modifier,
                                   TaskManagerGtk* task_manager);

  // Page sorting callback.
  static gint ComparePage(GtkTreeModel* model, GtkTreeIter* a,
                          GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_PAGE_COLUMN);
  }

  // Physical memory sorting callback.
  static gint ComparePhysicalMemory(GtkTreeModel* model, GtkTreeIter* a,
                                    GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_PHYSICAL_MEM_COLUMN);
  }

  // Shared memory sorting callback.
  static gint CompareSharedMemory(GtkTreeModel* model, GtkTreeIter* a,
                                  GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  }

  // Private memory sorting callback.
  static gint ComparePrivateMemory(GtkTreeModel* model, GtkTreeIter* a,
                                   GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  }

  // CPU sorting callback.
  static gint CompareCPU(GtkTreeModel* model, GtkTreeIter* a,
                         GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_CPU_COLUMN);
  }

  // Network sorting callback.
  static gint CompareNetwork(GtkTreeModel* model, GtkTreeIter* a,
                             GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_NET_COLUMN);
  }

  // Process ID sorting callback.
  static gint CompareProcessID(GtkTreeModel* model, GtkTreeIter* a,
                               GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_PROCESS_ID_COLUMN);
  }

  // WebCore Image Cache sorting callback.
  static gint CompareWebCoreImageCache(GtkTreeModel* model, GtkTreeIter* a,
                                       GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN);
  }

  // WebCore Scripts Cache sorting callback.
  static gint CompareWebCoreScriptsCache(GtkTreeModel* model, GtkTreeIter* a,
                                         GtkTreeIter* b,
                                         gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN);
  }

  // WebCore CSS Cache sorting callback.
  static gint CompareWebCoreCssCache(GtkTreeModel* model, GtkTreeIter* a,
                                     GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN);
  }

  // Goats Teleported sorting callback.
  static gint CompareGoatsTeleported(GtkTreeModel* model, GtkTreeIter* a,
                                     GtkTreeIter* b, gpointer task_manager) {
    return reinterpret_cast<TaskManagerGtk*>(task_manager)->
        CompareImpl(model, a, b, IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN);
  }

  // The task manager.
  TaskManager* task_manager_;

  // Our model.
  TaskManagerModel* model_;

  // The task manager dialog window.
  GtkWidget* dialog_;

  // The treeview that contains the process list.
  GtkWidget* treeview_;

  // The list of processes.
  GtkListStore* process_list_;
  GtkTreeModel* process_list_sort_;

  // The number of processes in |process_list_|.
  int process_count_;

  // The id of the |dialog_| destroy signal handler.
  gulong destroy_handler_id_;

  // The context menu controller.
  scoped_ptr<ContextMenuController> menu_controller_;

  GtkAccelGroup* accel_group_;

  // An open task manager window. There can only be one open at a time. This
  // is reset to NULL when the window is closed.
  static TaskManagerGtk* instance_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerGtk);
};

#endif  // CHROME_BROWSER_GTK_TASK_MANAGER_GTK_H_
