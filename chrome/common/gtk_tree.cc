// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gtk_tree.h"

#include "app/table_model.h"
#include "base/logging.h"

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

////////////////////////////////////////////////////////////////////////////////
//  TableAdapter

TableAdapter::TableAdapter(Delegate* delegate, GtkListStore* list_store,
                           TableModel* table_model)
    : delegate_(delegate), list_store_(list_store), table_model_(table_model) {
  if (table_model)
    table_model->SetObserver(this);
}

void TableAdapter::SetModel(TableModel* table_model) {
  table_model_ = table_model;
  table_model_->SetObserver(this);
}

void TableAdapter::AddNodeToList(int row) {
  GtkTreeIter iter;
  if (row == 0) {
    gtk_list_store_prepend(list_store_, &iter);
  } else {
    GtkTreeIter sibling;
    gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_), &sibling, NULL,
                                  row - 1);
    gtk_list_store_insert_after(list_store_, &iter, &sibling);
  }

  delegate_->SetColumnValues(row, &iter);
}

void TableAdapter::OnModelChanged() {
  delegate_->OnAnyModelUpdateStart();
  gtk_list_store_clear(list_store_);
  delegate_->OnModelChanged();
  for (int i = 0; i < table_model_->RowCount(); ++i)
    AddNodeToList(i);
  delegate_->OnAnyModelUpdate();
}

void TableAdapter::OnItemsChanged(int start, int length) {
  delegate_->OnAnyModelUpdateStart();
  GtkTreeIter iter;
  bool rv = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_), &iter,
                                          NULL, start);
  for (int i = 0; i < length; ++i) {
    if (!rv) {
      NOTREACHED();
      return;
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
  delegate_->OnAnyModelUpdateStart();
  GtkTreeIter iter;
  bool rv = gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(list_store_), &iter,
                                          NULL, start);
  for (int i = 0; i < length; ++i) {
    if (!rv) {
      NOTREACHED();
      return;
    }
    rv = gtk_list_store_remove(list_store_, &iter);
  }
  delegate_->OnAnyModelUpdate();
}

}  // namespace gtk_tree
