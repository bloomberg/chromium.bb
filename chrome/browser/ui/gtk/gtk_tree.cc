// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_tree.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/models/table_model.h"
#include "ui/gfx/gtk_util.h"

namespace gtk_tree {

gint GetRowNumForPath(GtkTreePath* path) {
  gint* indices = gtk_tree_path_get_indices(path);
  if (!indices) {
    NOTREACHED();
    return -1;
  }
  return indices[0];
}

gint GetRowNumForIter(GtkTreeModel* model, GtkTreeIter* iter) {
  GtkTreePath* path = gtk_tree_model_get_path(model, iter);
  int row = GetRowNumForPath(path);
  gtk_tree_path_free(path);
  return row;
}

gint GetTreeSortChildRowNumForPath(GtkTreeModel* sort_model,
                                   GtkTreePath* sort_path) {
  GtkTreePath *child_path = gtk_tree_model_sort_convert_path_to_child_path(
      GTK_TREE_MODEL_SORT(sort_model), sort_path);
  int row = GetRowNumForPath(child_path);
  gtk_tree_path_free(child_path);
  return row;
}

void SelectAndFocusRowNum(int row, GtkTreeView* tree_view) {
  GtkTreeModel* model = gtk_tree_view_get_model(tree_view);
  if (!model) {
    NOTREACHED();
    return;
  }
  GtkTreeIter iter;
  if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, row)) {
    NOTREACHED();
    return;
  }
  GtkTreePath* path = gtk_tree_model_get_path(model, &iter);
  gtk_tree_view_set_cursor(tree_view, path, NULL, FALSE);
  gtk_tree_path_free(path);
}

bool RemoveRecursively(GtkTreeStore* tree_store, GtkTreeIter* iter) {
  GtkTreeIter child;
  if (gtk_tree_model_iter_children(GTK_TREE_MODEL(tree_store), &child, iter)) {
    while (true) {
      if (!RemoveRecursively(tree_store, &child))
        break;
    }
  }
  return gtk_tree_store_remove(tree_store, iter);
}

void GetSelectedIndices(GtkTreeSelection* selection, std::set<int>* out) {
  GList* list = gtk_tree_selection_get_selected_rows(
      selection, NULL);
  GList* node;
  for (node = list; node != NULL; node = node->next) {
    out->insert(
        gtk_tree::GetRowNumForPath(static_cast<GtkTreePath*>(node->data)));
  }
  g_list_foreach(list, (GFunc)gtk_tree_path_free, NULL);
  g_list_free(list);
}

////////////////////////////////////////////////////////////////////////////////
//  TableAdapter

TableAdapter::TableAdapter(Delegate* delegate, GtkListStore* list_store,
                           ui::TableModel* table_model)
    : delegate_(delegate), list_store_(list_store), table_model_(table_model) {
  if (table_model)
    table_model->SetObserver(this);
}

void TableAdapter::SetModel(ui::TableModel* table_model) {
  table_model_ = table_model;
  table_model_->SetObserver(this);
}

bool TableAdapter::IsGroupRow(GtkTreeIter* iter) const {
  if (!table_model_->HasGroups())
    return false;
  gboolean is_header = false;
  gboolean is_separator = false;
  gtk_tree_model_get(GTK_TREE_MODEL(list_store_),
                     iter,
                     COL_IS_HEADER,
                     &is_header,
                     COL_IS_SEPARATOR,
                     &is_separator,
                     -1);
  return is_header || is_separator;
}

static int OffsetForGroupIndex(size_t group_index) {
  // Every group consists of a header and a separator row, and there is a blank
  // row between groups.
  return 3 * group_index + 2;
}

void TableAdapter::MapListStoreIndicesToModelRows(
    const std::set<int>& list_store_indices,
    RemoveRowsTableModel::Rows* model_rows) {
  if (!table_model_->HasGroups()) {
    for (std::set<int>::const_iterator it = list_store_indices.begin();
         it != list_store_indices.end();
         ++it) {
      model_rows->insert(*it);
    }
    return;
  }

  const ui::TableModel::Groups& groups = table_model_->GetGroups();
  ui::TableModel::Groups::const_iterator group_it = groups.begin();
  for (std::set<int>::const_iterator list_store_it = list_store_indices.begin();
       list_store_it != list_store_indices.end();
       ++list_store_it) {
    int list_store_index = *list_store_it;
    GtkTreeIter iter;
    bool rv = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_),
                                       &iter,
                                       NULL,
                                       list_store_index);
    if (!rv) {
      NOTREACHED();
      return;
    }
    int group = -1;
    gtk_tree_model_get(GTK_TREE_MODEL(list_store_),
                       &iter,
                       COL_GROUP_ID,
                       &group,
                       -1);
    while (group_it->id != group) {
      ++group_it;
      if (group_it == groups.end()) {
        NOTREACHED();
        return;
      }
    }
    int offset = OffsetForGroupIndex(group_it - groups.begin());
    model_rows->insert(list_store_index - offset);
  }
}

int TableAdapter::GetListStoreIndexForModelRow(int model_row) const {
  if (!table_model_->HasGroups())
    return model_row;
  int group = table_model_->GetGroupID(model_row);
  const ui::TableModel::Groups& groups = table_model_->GetGroups();
  for (ui::TableModel::Groups::const_iterator it = groups.begin();
       it != groups.end(); ++it) {
    if (it->id == group) {
      return model_row + OffsetForGroupIndex(it - groups.begin());
    }
  }
  NOTREACHED();
  return -1;
}

void TableAdapter::AddNodeToList(int row) {
  GtkTreeIter iter;
  int list_store_index = GetListStoreIndexForModelRow(row);
  if (list_store_index == 0) {
    gtk_list_store_prepend(list_store_, &iter);
  } else {
    GtkTreeIter sibling;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_), &sibling, NULL,
                                  list_store_index - 1);
    gtk_list_store_insert_after(list_store_, &iter, &sibling);
  }

  if (table_model_->HasGroups()) {
    gtk_list_store_set(list_store_,
                       &iter,
                       COL_WEIGHT, PANGO_WEIGHT_NORMAL,
                       COL_WEIGHT_SET, TRUE,
                       COL_GROUP_ID, table_model_->GetGroupID(row),
                       -1);
  }
  delegate_->SetColumnValues(row, &iter);
}

void TableAdapter::OnModelChanged() {
  delegate_->OnAnyModelUpdateStart();
  gtk_list_store_clear(list_store_);
  delegate_->OnModelChanged();

  if (table_model_->HasGroups()) {
    const ui::TableModel::Groups& groups = table_model_->GetGroups();
    for (ui::TableModel::Groups::const_iterator it = groups.begin();
         it != groups.end(); ++it) {
      GtkTreeIter iter;
      if (it != groups.begin()) {
        // Blank row between groups.
        gtk_list_store_append(list_store_, &iter);
        gtk_list_store_set(list_store_, &iter, COL_IS_HEADER, TRUE, -1);
      }
      // Group title.
      gtk_list_store_append(list_store_, &iter);
      gtk_list_store_set(list_store_,
                         &iter,
                         COL_WEIGHT,
                         PANGO_WEIGHT_BOLD,
                         COL_WEIGHT_SET,
                         TRUE,
                         COL_TITLE,
                         UTF16ToUTF8(it->title).c_str(),
                         COL_IS_HEADER,
                         TRUE,
                         -1);
      // Group separator.
      gtk_list_store_append(list_store_, &iter);
      gtk_list_store_set(list_store_,
                         &iter,
                         COL_IS_HEADER,
                         TRUE,
                         COL_IS_SEPARATOR,
                         TRUE,
                         -1);
    }
  }

  for (int i = 0; i < table_model_->RowCount(); ++i)
    AddNodeToList(i);
  delegate_->OnAnyModelUpdate();
}

void TableAdapter::OnItemsChanged(int start, int length) {
  if (length == 0)
    return;
  delegate_->OnAnyModelUpdateStart();
  int list_store_index = GetListStoreIndexForModelRow(start);
  GtkTreeIter iter;
  bool rv = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_),
                                          &iter,
                                          NULL,
                                          list_store_index);
  for (int i = 0; i < length; ++i) {
    if (!rv) {
      NOTREACHED();
      return;
    }
    while (IsGroupRow(&iter)) {
      rv = gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store_), &iter);
      if (!rv) {
        NOTREACHED();
        return;
      }
    }
    delegate_->SetColumnValues(start + i, &iter);
    rv = gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store_), &iter);
  }
  delegate_->OnAnyModelUpdate();
}

void TableAdapter::OnItemsAdded(int start, int length) {
  delegate_->OnAnyModelUpdateStart();
  for (int i = 0; i < length; ++i) {
    AddNodeToList(start + i);
  }
  delegate_->OnAnyModelUpdate();
}

void TableAdapter::OnItemsRemoved(int start, int length) {
  if (length == 0)
    return;
  delegate_->OnAnyModelUpdateStart();
  // When this method is called, the model has already removed the items, so
  // accessing items in the model from |start| on may not be possible anymore.
  // Therefore we use the item right before that, if it exists.
  int list_store_index = 0;
  if (start > 0)
    list_store_index = GetListStoreIndexForModelRow(start - 1) + 1;
  GtkTreeIter iter;
  bool rv = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_),
                                          &iter,
                                          NULL,
                                          list_store_index);
  if (!rv) {
    NOTREACHED();
    return;
  }
  for (int i = 0; i < length; ++i) {
    while (IsGroupRow(&iter)) {
      rv = gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store_), &iter);
      if (!rv) {
        NOTREACHED();
        return;
      }
    }
    gtk_list_store_remove(list_store_, &iter);
  }
  delegate_->OnAnyModelUpdate();
}

// static
gboolean TableAdapter::OnCheckRowIsSeparator(GtkTreeModel* model,
                                             GtkTreeIter* iter,
                                             gpointer user_data) {
  gboolean is_separator;
  gtk_tree_model_get(model,
                     iter,
                     COL_IS_SEPARATOR,
                     &is_separator,
                     -1);
  return is_separator;
}

// static
gboolean TableAdapter::OnSelectionFilter(GtkTreeSelection* selection,
                                         GtkTreeModel* model,
                                         GtkTreePath* path,
                                         gboolean path_currently_selected,
                                         gpointer user_data) {
  GtkTreeIter iter;
  if (!gtk_tree_model_get_iter(model, &iter, path)) {
    NOTREACHED();
    return TRUE;
  }
  gboolean is_header;
  gtk_tree_model_get(model, &iter, COL_IS_HEADER, &is_header, -1);
  return !is_header;
}

////////////////////////////////////////////////////////////////////////////////
//  TreeAdapter

TreeAdapter::TreeAdapter(Delegate* delegate, ui::TreeModel* tree_model)
    : delegate_(delegate),
      tree_model_(tree_model) {
  tree_store_ = gtk_tree_store_new(COL_COUNT,
                                   GDK_TYPE_PIXBUF,
                                   G_TYPE_STRING,
                                   G_TYPE_POINTER);
  tree_model->AddObserver(this);

  std::vector<SkBitmap> icons;
  tree_model->GetIcons(&icons);
  for (size_t i = 0; i < icons.size(); ++i) {
    pixbufs_.push_back(gfx::GdkPixbufFromSkBitmap(&icons[i]));
  }
}

TreeAdapter::~TreeAdapter() {
  g_object_unref(tree_store_);
  for (size_t i = 0; i < pixbufs_.size(); ++i)
    g_object_unref(pixbufs_[i]);
}

void TreeAdapter::Init() {
  gtk_tree_store_clear(tree_store_);
  Fill(NULL, tree_model_->GetRoot());
}


ui::TreeModelNode* TreeAdapter::GetNode(GtkTreeIter* iter) {
  ui::TreeModelNode* node;
  gtk_tree_model_get(GTK_TREE_MODEL(tree_store_), iter,
                     COL_NODE_PTR, &node,
                     -1);
  return node;
}

void TreeAdapter::FillRow(GtkTreeIter* iter, ui::TreeModelNode* node) {
  GdkPixbuf* pixbuf = NULL;
  int icon_index = tree_model_->GetIconIndex(node);
  if (icon_index >= 0 && icon_index < static_cast<int>(pixbufs_.size()))
    pixbuf = pixbufs_[icon_index];
  else
    pixbuf = GtkThemeService::GetFolderIcon(true);
  gtk_tree_store_set(tree_store_, iter,
                     COL_ICON, pixbuf,
                     COL_TITLE, UTF16ToUTF8(node->GetTitle()).c_str(),
                     COL_NODE_PTR, node,
                     -1);
}

void TreeAdapter::Fill(GtkTreeIter* parent_iter,
                       ui::TreeModelNode* parent_node) {
  if (parent_iter)
    FillRow(parent_iter, parent_node);
  GtkTreeIter iter;
  int child_count = tree_model_->GetChildCount(parent_node);
  for (int i = 0; i < child_count; ++i) {
    ui::TreeModelNode* node = tree_model_->GetChild(parent_node, i);
    gtk_tree_store_append(tree_store_, &iter, parent_iter);
    Fill(&iter, node);
  }
}

GtkTreePath* TreeAdapter::GetTreePath(ui::TreeModelNode* node) {
  GtkTreePath* path = gtk_tree_path_new();
  ui::TreeModelNode* parent = node;
  while (parent) {
    parent = tree_model_->GetParent(parent);
    if (parent) {
      int idx = tree_model_->GetIndexOf(parent, node);
      gtk_tree_path_prepend_index(path, idx);
      node = parent;
    }
  }
  return path;
}

bool TreeAdapter::GetTreeIter(ui::TreeModelNode* node, GtkTreeIter* iter) {
  GtkTreePath* path = GetTreePath(node);
  bool rv = false;
  // Check the path ourselves since gtk_tree_model_get_iter prints a warning if
  // given an empty path.  The path will be empty when it points to the root
  // node and we are using SetRootShown(false).
  if (gtk_tree_path_get_depth(path) > 0)
    rv = gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store_), iter, path);
  gtk_tree_path_free(path);
  return rv;
}

void TreeAdapter::TreeNodesAdded(ui::TreeModel* model,
                                 ui::TreeModelNode* parent,
                                 int start,
                                 int count) {
  delegate_->OnAnyModelUpdateStart();
  GtkTreeIter parent_iter;
  GtkTreeIter* parent_iter_ptr = NULL;
  GtkTreeIter iter;
  if (GetTreeIter(parent, &parent_iter))
    parent_iter_ptr = &parent_iter;
  for (int i = 0; i < count; ++i) {
    gtk_tree_store_insert(tree_store_, &iter, parent_iter_ptr, start + i);
    Fill(&iter, tree_model_->GetChild(parent, start + i));
  }
  delegate_->OnAnyModelUpdate();
}

void TreeAdapter::TreeNodesRemoved(ui::TreeModel* model,
                                   ui::TreeModelNode* parent,
                                   int start,
                                   int count) {
  delegate_->OnAnyModelUpdateStart();
  GtkTreeIter iter;
  GtkTreePath* path = GetTreePath(parent);
  gtk_tree_path_append_index(path, start);
  gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store_), &iter, path);
  gtk_tree_path_free(path);
  for (int i = 0; i < count; ++i) {
    RemoveRecursively(tree_store_, &iter);
  }
  delegate_->OnAnyModelUpdate();
}

void TreeAdapter::TreeNodeChanged(ui::TreeModel* model,
                                  ui::TreeModelNode* node) {
  delegate_->OnAnyModelUpdateStart();
  GtkTreeIter iter;
  if (GetTreeIter(node, &iter))
    FillRow(&iter, node);
  delegate_->OnAnyModelUpdate();
}

}  // namespace gtk_tree
