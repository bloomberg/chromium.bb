// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/task_manager_gtk.h"

#include <gdk/gdkkeysyms.h>

#include <algorithm>
#include <vector>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/common/gtk_tree.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"

namespace {

// The task manager window default size.
const int kDefaultWidth = 460;
const int kDefaultHeight = 270;

// The resource id for the 'End process' button.
const gint kTaskManagerResponseKill = 1;

// The resource id for the 'Stats for nerds' link button.
const gint kTaskManagerAboutMemoryLink = 2;

enum TaskManagerColumn {
  kTaskManagerIcon,
  kTaskManagerPage,
  kTaskManagerSharedMem,
  kTaskManagerPrivateMem,
  kTaskManagerCPU,
  kTaskManagerNetwork,
  kTaskManagerProcessID,
  kTaskManagerWebCoreImageCache,
  kTaskManagerWebCoreScriptsCache,
  kTaskManagerWebCoreCssCache,
  kTaskManagerGoatsTeleported,
  kTaskManagerColumnCount,
};

TaskManagerColumn TaskManagerResourceIDToColumnID(int id) {
  switch (id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:
      return kTaskManagerPage;
    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:
      return kTaskManagerSharedMem;
    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:
      return kTaskManagerPrivateMem;
    case IDS_TASK_MANAGER_CPU_COLUMN:
      return kTaskManagerCPU;
    case IDS_TASK_MANAGER_NET_COLUMN:
      return kTaskManagerNetwork;
    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:
      return kTaskManagerProcessID;
    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
      return kTaskManagerWebCoreImageCache;
    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
      return kTaskManagerWebCoreScriptsCache;
    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
      return kTaskManagerWebCoreCssCache;
    case IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN:
      return kTaskManagerGoatsTeleported;
    default:
      NOTREACHED();
      return static_cast<TaskManagerColumn>(-1);
  }
}

int TaskManagerColumnIDToResourceID(int id) {
  switch (id) {
    case kTaskManagerPage:
      return IDS_TASK_MANAGER_PAGE_COLUMN;
    case kTaskManagerSharedMem:
      return IDS_TASK_MANAGER_SHARED_MEM_COLUMN;
    case kTaskManagerPrivateMem:
      return IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN;
    case kTaskManagerCPU:
      return IDS_TASK_MANAGER_CPU_COLUMN;
    case kTaskManagerNetwork:
      return IDS_TASK_MANAGER_NET_COLUMN;
    case kTaskManagerProcessID:
      return IDS_TASK_MANAGER_PROCESS_ID_COLUMN;
    case kTaskManagerWebCoreImageCache:
      return IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN;
    case kTaskManagerWebCoreScriptsCache:
      return IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN;
    case kTaskManagerWebCoreCssCache:
      return IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN;
    case kTaskManagerGoatsTeleported:
      return IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN;
    default:
      NOTREACHED();
      return -1;
  }
}

// Should be used for all gtk_tree_view functions that require a column index on
// input.
//
// We need colid - 1 because the gtk_tree_view function is asking for the
// column index, not the column id, and both kTaskManagerIcon and
// kTaskManagerPage are in the same column index, so all column IDs are off by
// one.
int TreeViewColumnIndexFromID(TaskManagerColumn colid) {
  return colid - 1;
}

// Shows or hides a treeview column.
void TreeViewColumnSetVisible(GtkWidget* treeview, TaskManagerColumn colid,
                              bool visible) {
  GtkTreeViewColumn* column = gtk_tree_view_get_column(
      GTK_TREE_VIEW(treeview), TreeViewColumnIndexFromID(colid));
  gtk_tree_view_column_set_visible(column, visible);
}

bool TreeViewColumnIsVisible(GtkWidget* treeview, TaskManagerColumn colid) {
  GtkTreeViewColumn* column = gtk_tree_view_get_column(
      GTK_TREE_VIEW(treeview), TreeViewColumnIndexFromID(colid));
  return gtk_tree_view_column_get_visible(column);
}

void TreeViewInsertColumnWithPixbuf(GtkWidget* treeview, int resid) {
  int colid = TaskManagerResourceIDToColumnID(resid);
  GtkTreeViewColumn* column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(column,
                                 l10n_util::GetStringUTF8(resid).c_str());
  GtkCellRenderer* image_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, image_renderer, FALSE);
  gtk_tree_view_column_add_attribute(column, image_renderer,
                                     "pixbuf", kTaskManagerIcon);
  GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, text_renderer, "text", colid);
  gtk_tree_view_column_set_resizable(column, TRUE);
  // This is temporary: we'll turn expanding off after getting the size.
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
  gtk_tree_view_column_set_sort_column_id(column, colid);
}

// Inserts a column with a column id of |colid| and |name|.
void TreeViewInsertColumnWithName(GtkWidget* treeview,
                                  TaskManagerColumn colid, const char* name) {
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1,
                                              name, renderer,
                                              "text", colid,
                                              NULL);
  GtkTreeViewColumn* column = gtk_tree_view_get_column(
      GTK_TREE_VIEW(treeview), TreeViewColumnIndexFromID(colid));
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, colid);
}

// Loads the column name from |resid| and uses the corresponding
// TaskManagerColumn value as the column id to insert into the treeview.
void TreeViewInsertColumn(GtkWidget* treeview, int resid) {
  TreeViewInsertColumnWithName(treeview, TaskManagerResourceIDToColumnID(resid),
                               l10n_util::GetStringUTF8(resid).c_str());
}

// Set the current width of the column without forcing a minimum width as
// gtk_tree_view_column_set_fixed_width() would. This would basically be
// gtk_tree_view_column_set_width() except that there is no such function.
void TreeViewColumnSetWidth(GtkTreeViewColumn* column, gint width) {
  column->width = width;
  column->resized_width = width;
  column->use_resized_width = TRUE;
  // Needed for use_resized_width to be effective.
  gtk_widget_queue_resize(column->tree_view);
}

}  // namespace

class TaskManagerGtk::ContextMenuController : public MenuGtk::Delegate {
 public:
  explicit ContextMenuController(TaskManagerGtk* task_manager)
      : task_manager_(task_manager) {
    menu_.reset(new MenuGtk(this, false));
    for (int i = kTaskManagerPage; i < kTaskManagerColumnCount; i++) {
      menu_->AppendCheckMenuItemWithLabel(
          i, l10n_util::GetStringUTF8(TaskManagerColumnIDToResourceID(i)));
    }
  }

  virtual ~ContextMenuController() {}

  void RunMenu() {
    menu_->PopupAsContext(gtk_get_current_event_time());
  }

  void Cancel() {
    task_manager_ = NULL;
    menu_->Cancel();
  }

 private:
  // MenuGtk::Delegate implementation:
  virtual bool IsCommandEnabled(int command_id) const {
    if (!task_manager_)
      return false;

    return true;
  }

  virtual bool IsItemChecked(int command_id) const {
    if (!task_manager_)
      return false;

    TaskManagerColumn colid = static_cast<TaskManagerColumn>(command_id);
    return TreeViewColumnIsVisible(task_manager_->treeview_, colid);
  }

  virtual void ExecuteCommand(int command_id) {
    if (!task_manager_)
      return;

    TaskManagerColumn colid = static_cast<TaskManagerColumn>(command_id);
    bool visible = !TreeViewColumnIsVisible(task_manager_->treeview_, colid);
    TreeViewColumnSetVisible(task_manager_->treeview_, colid, visible);
  }

  // The context menu.
  scoped_ptr<MenuGtk> menu_;

  // The TaskManager the context menu was brought up for. Set to NULL when the
  // menu is canceled.
  TaskManagerGtk* task_manager_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuController);
};

TaskManagerGtk::TaskManagerGtk()
  : task_manager_(TaskManager::GetInstance()),
    model_(TaskManager::GetInstance()->model()),
    dialog_(NULL),
    treeview_(NULL),
    process_list_(NULL),
    process_count_(0) {
  Init();
}

// static
TaskManagerGtk* TaskManagerGtk::instance_ = NULL;

TaskManagerGtk::~TaskManagerGtk() {
  task_manager_->OnWindowClosed();
  model_->RemoveObserver(this);

  gtk_accel_group_disconnect_key(accel_group_, GDK_w, GDK_CONTROL_MASK);
  gtk_window_remove_accel_group(GTK_WINDOW(dialog_), accel_group_);
  g_object_unref(accel_group_);
  accel_group_ = NULL;

  // Disconnect the destroy signal so it doesn't delete |this|.
  g_signal_handler_disconnect(G_OBJECT(dialog_), destroy_handler_id_);
  gtk_widget_destroy(dialog_);
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, TaskManagerModelObserver implementation:

void TaskManagerGtk::OnModelChanged() {
  // Nothing to do.
}

void TaskManagerGtk::OnItemsChanged(int start, int length) {
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &iter,
                                NULL, start);

  for (int i = start; i < start + length; i++) {
    SetRowDataFromModel(i, &iter);
    gtk_tree_model_iter_next(GTK_TREE_MODEL(process_list_), &iter);
  }
}

void TaskManagerGtk::OnItemsAdded(int start, int length) {
  GtkTreeIter iter;
  if (start == 0) {
    gtk_list_store_prepend(process_list_, &iter);
  } else if (start >= process_count_) {
    gtk_list_store_append(process_list_, &iter);
  } else {
    GtkTreeIter sibling;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &sibling,
                                  NULL, start);
    gtk_list_store_insert_before(process_list_, &iter, &sibling);
  }

  SetRowDataFromModel(start, &iter);

  for (int i = start + 1; i < start + length; i++) {
    gtk_list_store_insert_after(process_list_, &iter, &iter);
    SetRowDataFromModel(i, &iter);
  }

  process_count_ += length;
}

void TaskManagerGtk::OnItemsRemoved(int start, int length) {
  GtkTreeIter iter;
  gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(process_list_), &iter,
                                NULL, start);

  for (int i = 0; i < length; i++) {
    // |iter| is moved to the next valid node when the current node is removed.
    gtk_list_store_remove(process_list_, &iter);
  }

  process_count_ -= length;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, public:

// static
void TaskManagerGtk::Show() {
  if (instance_) {
    // If there's a Task manager window open already, just activate it.
    gtk_window_present(GTK_WINDOW(instance_->dialog_));
  } else {
    instance_ = new TaskManagerGtk;
    instance_->model_->StartUpdating();
  }
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerGtk, private:

void TaskManagerGtk::Init() {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_TASK_MANAGER_TITLE).c_str(),
      // Task Manager window is shared between all browsers.
      NULL,
      GTK_DIALOG_NO_SEPARATOR,
      l10n_util::GetStringUTF8(IDS_TASK_MANAGER_KILL).c_str(),
      kTaskManagerResponseKill,
      NULL);

  // Allow browser windows to go in front of the task manager dialog in
  // metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);

  // The response button should not be sensitive when the dialog is first opened
  // because the selection is initially empty.
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog_),
                                    kTaskManagerResponseKill, FALSE);

  GtkWidget* link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_TASK_MANAGER_ABOUT_MEMORY_LINK).c_str());
  gtk_dialog_add_action_widget(GTK_DIALOG(dialog_), link,
                               kTaskManagerAboutMemoryLink);

  // Setting the link widget to secondary positions the button on the left side
  // of the action area (vice versa for RTL layout).
  gtk_button_box_set_child_secondary(
      GTK_BUTTON_BOX(GTK_DIALOG(dialog_)->action_area), link, TRUE);

  ConnectAccelerators();

  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  destroy_handler_id_ = g_signal_connect(G_OBJECT(dialog_), "destroy",
                                         G_CALLBACK(OnDestroy), this);
  g_signal_connect(G_OBJECT(dialog_), "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(G_OBJECT(dialog_), "button-release-event",
                   G_CALLBACK(OnButtonReleaseEvent), this);
  gtk_widget_add_events(dialog_,
                        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  // Wrap the treeview widget in a scrolled window in order to have a frame.
  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), scrolled);

  CreateTaskManagerTreeview();
  gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(treeview_), TRUE);
  gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(treeview_),
                               GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
  g_signal_connect(G_OBJECT(treeview_), "button-press-event",
                   G_CALLBACK(OnButtonPressEvent), this);
  gtk_widget_add_events(treeview_,
                        GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK);

  // |selection| is owned by |treeview_|.
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
  g_signal_connect(G_OBJECT(selection), "changed",
                   G_CALLBACK(OnSelectionChanged), this);

  gtk_container_add(GTK_CONTAINER(scrolled), treeview_);

  SetInitialDialogSize();
  gtk_widget_show_all(dialog_);

  model_->AddObserver(this);
}

void TaskManagerGtk::SetInitialDialogSize() {
  // Hook up to the realize event so we can size the page column to the
  // size of the leftover space after packing the other columns.
  g_signal_connect(G_OBJECT(treeview_), "realize",
                   G_CALLBACK(OnTreeViewRealize), this);
  // If we previously saved the dialog's bounds, use them.
  if (g_browser_process->local_state()) {
    const DictionaryValue* placement_pref =
        g_browser_process->local_state()->GetDictionary(
            prefs::kTaskManagerWindowPlacement);
    int top = 0, left = 0, bottom = 1, right = 1;
    if (placement_pref &&
        placement_pref->GetInteger(L"top", &top) &&
        placement_pref->GetInteger(L"left", &left) &&
        placement_pref->GetInteger(L"bottom", &bottom) &&
        placement_pref->GetInteger(L"right", &right)) {
      gtk_window_resize(GTK_WINDOW(dialog_),
                        std::max(1, right - left),
                        std::max(1, bottom - top));
      return;
    }
  }

  // Otherwise, just set a default size (GTK will override this if it's not
  // large enough to hold the window's contents).
  gtk_window_set_default_size(
      GTK_WINDOW(dialog_), kDefaultWidth, kDefaultHeight);
}

void TaskManagerGtk::ConnectAccelerators() {
  accel_group_ = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(dialog_), accel_group_);

  gtk_accel_group_connect(accel_group_,
                          GDK_w, GDK_CONTROL_MASK, GtkAccelFlags(0),
                          g_cclosure_new(G_CALLBACK(OnGtkAccelerator),
                                         this, NULL));
}

void TaskManagerGtk::CreateTaskManagerTreeview() {
  process_list_ = gtk_list_store_new(kTaskManagerColumnCount,
      GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  // Support sorting on all columns.
  process_list_sort_ = gtk_tree_model_sort_new_with_model(
      GTK_TREE_MODEL(process_list_));
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerPage,
                                  ComparePage, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerSharedMem,
                                  CompareSharedMemory, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerPrivateMem,
                                  ComparePrivateMemory, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerCPU,
                                  CompareCPU, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerNetwork,
                                  CompareNetwork, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerProcessID,
                                  CompareProcessID, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerWebCoreImageCache,
                                  CompareWebCoreImageCache, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerWebCoreScriptsCache,
                                  CompareWebCoreScriptsCache, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerWebCoreCssCache,
                                  CompareWebCoreCssCache, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(process_list_sort_),
                                  kTaskManagerGoatsTeleported,
                                  CompareGoatsTeleported, this, NULL);
  treeview_ = gtk_tree_view_new_with_model(process_list_sort_);

  // Insert all the columns.
  TreeViewInsertColumnWithPixbuf(treeview_, IDS_TASK_MANAGER_PAGE_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_CPU_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_NET_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_PROCESS_ID_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN);
  TreeViewInsertColumn(treeview_,
                       IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN);
  TreeViewInsertColumn(treeview_, IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN);

  // Hide some columns by default.
  TreeViewColumnSetVisible(treeview_, kTaskManagerPrivateMem, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerProcessID, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerWebCoreImageCache, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerWebCoreScriptsCache, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerWebCoreCssCache, false);
  TreeViewColumnSetVisible(treeview_, kTaskManagerGoatsTeleported, false);

  g_object_unref(process_list_);
  g_object_unref(process_list_sort_);
}

std::string TaskManagerGtk::GetModelText(int row, int col_id) {
  switch (col_id) {
    case IDS_TASK_MANAGER_PAGE_COLUMN:  // Process
      return WideToUTF8(model_->GetResourceTitle(row));

    case IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourcePrivateMemory(row));

    case IDS_TASK_MANAGER_SHARED_MEM_COLUMN:  // Memory
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceSharedMemory(row));

    case IDS_TASK_MANAGER_CPU_COLUMN:  // CPU
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceCPUUsage(row));

    case IDS_TASK_MANAGER_NET_COLUMN:  // Net
      return WideToUTF8(model_->GetResourceNetworkUsage(row));

    case IDS_TASK_MANAGER_PROCESS_ID_COLUMN:  // Process ID
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceProcessId(row));

    case IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceWebCoreImageCacheSize(row));

    case IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceWebCoreScriptsCacheSize(row));

    case IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN:
      if (!model_->IsResourceFirstInGroup(row))
        return std::string();
      return WideToUTF8(model_->GetResourceWebCoreCSSCacheSize(row));

    case IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN:  // Goats Teleported!
      return WideToUTF8(model_->GetResourceGoatsTeleported(row));

    default:
      return WideToUTF8(model_->GetResourceStatsValue(row, col_id));
  }
}

GdkPixbuf* TaskManagerGtk::GetModelIcon(int row) {
  SkBitmap icon = model_->GetResourceIcon(row);
  if (icon.pixelRef() ==
      ResourceBundle::GetSharedInstance().GetBitmapNamed(
          IDR_DEFAULT_FAVICON)->pixelRef()) {
    return static_cast<GdkPixbuf*>(g_object_ref(
        GtkThemeProvider::GetDefaultFavicon(true)));
  }

  return gfx::GdkPixbufFromSkBitmap(&icon);
}

void TaskManagerGtk::SetRowDataFromModel(int row, GtkTreeIter* iter) {
  GdkPixbuf* icon = GetModelIcon(row);
  std::string page = GetModelText(row, IDS_TASK_MANAGER_PAGE_COLUMN);
  std::string shared_mem = GetModelText(
      row, IDS_TASK_MANAGER_SHARED_MEM_COLUMN);
  std::string priv_mem = GetModelText(row, IDS_TASK_MANAGER_PRIVATE_MEM_COLUMN);
  std::string cpu = GetModelText(row, IDS_TASK_MANAGER_CPU_COLUMN);
  std::string net = GetModelText(row, IDS_TASK_MANAGER_NET_COLUMN);
  std::string procid = GetModelText(row, IDS_TASK_MANAGER_PROCESS_ID_COLUMN);

  // Querying the WebCore metrics is slow as it has to do IPC, so only do it
  // when the columns are visible.
  std::string wk_img_cache;
  if (TreeViewColumnIsVisible(treeview_, kTaskManagerWebCoreImageCache))
    wk_img_cache = GetModelText(
        row, IDS_TASK_MANAGER_WEBCORE_IMAGE_CACHE_COLUMN);
  std::string wk_scripts_cache;
  if (TreeViewColumnIsVisible(treeview_, kTaskManagerWebCoreScriptsCache))
    wk_scripts_cache = GetModelText(
        row, IDS_TASK_MANAGER_WEBCORE_SCRIPTS_CACHE_COLUMN);
  std::string wk_css_cache;
  if (TreeViewColumnIsVisible(treeview_, kTaskManagerWebCoreCssCache))
    wk_css_cache = GetModelText(
        row, IDS_TASK_MANAGER_WEBCORE_CSS_CACHE_COLUMN);

  std::string goats = GetModelText(
      row, IDS_TASK_MANAGER_GOATS_TELEPORTED_COLUMN);
  gtk_list_store_set(process_list_, iter,
                     kTaskManagerIcon, icon,
                     kTaskManagerPage, page.c_str(),
                     kTaskManagerSharedMem, shared_mem.c_str(),
                     kTaskManagerPrivateMem, priv_mem.c_str(),
                     kTaskManagerCPU, cpu.c_str(),
                     kTaskManagerNetwork, net.c_str(),
                     kTaskManagerProcessID, procid.c_str(),
                     kTaskManagerWebCoreImageCache, wk_img_cache.c_str(),
                     kTaskManagerWebCoreScriptsCache, wk_scripts_cache.c_str(),
                     kTaskManagerWebCoreCssCache, wk_css_cache.c_str(),
                     kTaskManagerGoatsTeleported, goats.c_str(),
                     -1);
  g_object_unref(icon);
}

void TaskManagerGtk::KillSelectedProcesses() {
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));

  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(selection, &model);
  for (GList* item = paths; item; item = item->next) {
    GtkTreePath* path = gtk_tree_model_sort_convert_path_to_child_path(
        GTK_TREE_MODEL_SORT(process_list_sort_),
        reinterpret_cast<GtkTreePath*>(item->data));
    int row = gtk_tree::GetRowNumForPath(path);
    gtk_tree_path_free(path);
    task_manager_->KillProcess(row);
  }
  g_list_foreach(paths, reinterpret_cast<GFunc>(gtk_tree_path_free), NULL);
  g_list_free(paths);
}

void TaskManagerGtk::ShowContextMenu() {
  if (!menu_controller_.get())
    menu_controller_.reset(new ContextMenuController(this));

  menu_controller_->RunMenu();
}

void TaskManagerGtk::ActivateFocusedTab() {
  GtkTreeSelection* selection = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));

  // If the user has just double clicked, only one item is selected.
  GtkTreeModel* model;
  GList* selected = gtk_tree_selection_get_selected_rows(selection, &model);
  if (selected) {
    GtkTreePath* path = gtk_tree_model_sort_convert_path_to_child_path(
        GTK_TREE_MODEL_SORT(process_list_sort_),
        reinterpret_cast<GtkTreePath*>(selected->data));
    int row = gtk_tree::GetRowNumForPath(path);
    gtk_tree_path_free(path);
    task_manager_->ActivateProcess(row);
  }
  g_list_foreach(selected, reinterpret_cast<GFunc>(gtk_tree_path_free), NULL);
  g_list_free(selected);
}

void TaskManagerGtk::OnLinkActivated() {
  task_manager_->OpenAboutMemory();
}

gint TaskManagerGtk::CompareImpl(GtkTreeModel* model, GtkTreeIter* a,
                                 GtkTreeIter* b, int id) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  return model_->CompareValues(row1, row2, id);
}

// static
void TaskManagerGtk::OnDestroy(GtkDialog* dialog,
                               TaskManagerGtk* task_manager) {
  instance_ = NULL;
  delete task_manager;
}

// static
void TaskManagerGtk::OnResponse(GtkDialog* dialog, gint response_id,
                                TaskManagerGtk* task_manager) {
  if (response_id == GTK_RESPONSE_DELETE_EVENT) {
    // Store the dialog's size so we can restore it the next time it's opened.
    if (g_browser_process->local_state()) {
      gint x = 0, y = 0, width = 1, height = 1;
      gtk_window_get_position(GTK_WINDOW(dialog), &x, &y);
      gtk_window_get_size(GTK_WINDOW(dialog), &width, &height);

      DictionaryValue* placement_pref =
          g_browser_process->local_state()->GetMutableDictionary(
              prefs::kTaskManagerWindowPlacement);
      // Note that we store left/top for consistency with Windows, but that we
      // *don't* restore them.
      placement_pref->SetInteger(L"left", x);
      placement_pref->SetInteger(L"top", y);
      placement_pref->SetInteger(L"right", x + width);
      placement_pref->SetInteger(L"bottom", y + height);
      placement_pref->SetBoolean(L"maximized", false);
    }

    instance_ = NULL;
    delete task_manager;
  } else if (response_id == kTaskManagerResponseKill) {
    task_manager->KillSelectedProcesses();
  } else if (response_id == kTaskManagerAboutMemoryLink) {
    task_manager->OnLinkActivated();
  }
}

// static
void TaskManagerGtk::OnTreeViewRealize(GtkTreeView* treeview,
                                       TaskManagerGtk* task_manager) {
  // Four columns show by default: the page column, the memory column, the
  // CPU column, and the network column. Initially we set the page column to
  // take all the extra space, with the other columns being sized to fit the
  // column names. Here we turn off the expand property of the first column
  // (to make the table behave sanely when the user resizes columns) and set
  // the effective sizes of all four default columns to the automatically
  // chosen size before any rows are added. This causes them to stay at that
  // size even if the data would overflow, preventing a horizontal scroll
  // bar from appearing due to the row data.
  const TaskManagerColumn dfl_columns[] = {kTaskManagerNetwork, kTaskManagerCPU,
                                           kTaskManagerSharedMem};
  GtkTreeViewColumn* column = NULL;
  gint width;
  for (size_t i = 0; i < arraysize(dfl_columns); ++i) {
    column = gtk_tree_view_get_column(treeview,
        TreeViewColumnIndexFromID(dfl_columns[i]));
    width = gtk_tree_view_column_get_width(column);
    TreeViewColumnSetWidth(column, width);
  }
  // Do the page column separately since it's a little different.
  column = gtk_tree_view_get_column(treeview,
      TreeViewColumnIndexFromID(kTaskManagerPage));
  width = gtk_tree_view_column_get_width(column);
  // Turn expanding back off to make resizing columns behave sanely.
  gtk_tree_view_column_set_expand(column, FALSE);
  TreeViewColumnSetWidth(column, width);
}

// static
void TaskManagerGtk::OnSelectionChanged(GtkTreeSelection* selection,
                                        TaskManagerGtk* task_manager) {
  bool selection_contains_browser_process = false;

  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(selection, &model);
  for (GList* item = paths; item; item = item->next) {
    GtkTreePath* path = gtk_tree_model_sort_convert_path_to_child_path(
        GTK_TREE_MODEL_SORT(task_manager->process_list_sort_),
        reinterpret_cast<GtkTreePath*>(item->data));
    int row = gtk_tree::GetRowNumForPath(path);
    gtk_tree_path_free(path);
    if (task_manager->task_manager_->IsBrowserProcess(row)) {
      selection_contains_browser_process = true;
      break;
    }
  }
  g_list_foreach(paths, reinterpret_cast<GFunc>(gtk_tree_path_free), NULL);
  g_list_free(paths);

  bool sensitive = (paths != NULL) && !selection_contains_browser_process;
  gtk_dialog_set_response_sensitive(GTK_DIALOG(task_manager->dialog_),
                                    kTaskManagerResponseKill, sensitive);
}

// static
gboolean TaskManagerGtk::OnButtonPressEvent(GtkWidget* widget,
                                            GdkEventButton* event,
                                            TaskManagerGtk* task_manager) {
  if (event->type == GDK_2BUTTON_PRESS)
    task_manager->ActivateFocusedTab();

  return FALSE;
}

// static
gboolean TaskManagerGtk::OnButtonReleaseEvent(GtkWidget* widget,
                                              GdkEventButton* event,
                                              TaskManagerGtk* task_manager) {
  if (event->button == 3)
    task_manager->ShowContextMenu();

  return FALSE;
}

// static
gboolean TaskManagerGtk::OnGtkAccelerator(GtkAccelGroup* accel_group,
                                          GObject* acceleratable,
                                          guint keyval,
                                          GdkModifierType modifier,
                                          TaskManagerGtk* task_manager) {
  if (keyval == GDK_w && modifier == GDK_CONTROL_MASK) {
    // The GTK_RESPONSE_DELETE_EVENT response must be sent before the widget
    // is destroyed.  The deleted object will receive gtk signals otherwise.
    gtk_dialog_response(GTK_DIALOG(task_manager->dialog_),
                        GTK_RESPONSE_DELETE_EVENT);
  }

  return TRUE;
}
