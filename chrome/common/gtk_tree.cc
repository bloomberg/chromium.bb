// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gtk_tree.h"

#include "app/gfx/gtk_util.h"
#include "app/table_model.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"

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

////////////////////////////////////////////////////////////////////////////////
//  TreeAdapter

TreeAdapter::TreeAdapter(Delegate* delegate, TreeModel* tree_model)
    : delegate_(delegate),
      tree_model_(tree_model) {
  tree_store_ = gtk_tree_store_new(COL_COUNT,
                                   GDK_TYPE_PIXBUF,
                                   G_TYPE_STRING,
                                   G_TYPE_POINTER);
  tree_model->SetObserver(this);

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


TreeModelNode* TreeAdapter::GetNode(GtkTreeIter* iter) {
  TreeModelNode* node;
  gtk_tree_model_get(GTK_TREE_MODEL(tree_store_), iter,
                     COL_NODE_PTR, &node,
                     -1);
  return node;
}

void TreeAdapter::FillRow(GtkTreeIter* iter, TreeModelNode* node) {
  GdkPixbuf* pixbuf = NULL;
  int icon_index = tree_model_->GetIconIndex(node);
  if (icon_index >= 0 && icon_index < static_cast<int>(pixbufs_.size()))
    pixbuf = pixbufs_[icon_index];
  else
    pixbuf = GtkThemeProvider::GetFolderIcon(true);
  gtk_tree_store_set(tree_store_, iter,
                     COL_ICON, pixbuf,
                     COL_TITLE, WideToUTF8(node->GetTitle()).c_str(),
                     COL_NODE_PTR, node,
                     -1);
}

void TreeAdapter::Fill(GtkTreeIter* parent_iter, TreeModelNode* parent_node) {
  if (parent_iter)
    FillRow(parent_iter, parent_node);
  GtkTreeIter iter;
  int child_count = tree_model_->GetChildCount(parent_node);
  for (int i = 0; i < child_count; ++i) {
    TreeModelNode* node = tree_model_->GetChild(parent_node, i);
    gtk_tree_store_append(tree_store_, &iter, parent_iter);
    Fill(&iter, node);
  }
}

GtkTreePath* TreeAdapter::GetTreePath(TreeModelNode* node) {
  GtkTreePath* path = gtk_tree_path_new();
  TreeModelNode* parent = node;
  while (parent) {
    parent = tree_model_->GetParent(parent);
    if (parent) {
      int idx = tree_model_->IndexOfChild(parent, node);
      gtk_tree_path_prepend_index(path, idx);
      node = parent;
    }
  }
  return path;
}

bool TreeAdapter::GetTreeIter(TreeModelNode* node, GtkTreeIter* iter) {
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

void TreeAdapter::TreeNodesAdded(TreeModel* model,
                                 TreeModelNode* parent,
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

void TreeAdapter::TreeNodesRemoved(TreeModel* model,
                                   TreeModelNode* parent,
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

void TreeAdapter::TreeNodeChildrenReordered(TreeModel* model,
                                            TreeModelNode* parent) {
  NOTIMPLEMENTED();
}

void TreeAdapter::TreeNodeChanged(TreeModel* model, TreeModelNode* node) {
  delegate_->OnAnyModelUpdateStart();
  GtkTreeIter iter;
  if (GetTreeIter(node, &iter))
    FillRow(&iter, node);
  delegate_->OnAnyModelUpdate();
}

}  // namespace gtk_tree
