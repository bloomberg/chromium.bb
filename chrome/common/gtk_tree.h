// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GTK_TREE_H_
#define CHROME_COMMON_GTK_TREE_H_

#include <gtk/gtk.h>
#include <vector>

#include "app/table_model_observer.h"
#include "app/tree_model.h"
#include "base/basictypes.h"

class TableModel;

namespace gtk_tree {

// Get the row number corresponding to |path|.
gint GetRowNumForPath(GtkTreePath* path);

// Get the row number corresponding to |iter|.
gint GetRowNumForIter(GtkTreeModel* model, GtkTreeIter* iter);

// Get the row number in the child tree model corresponding to |sort_path| in
// the parent tree model.
gint GetTreeSortChildRowNumForPath(GtkTreeModel* sort_model,
                                   GtkTreePath* sort_path);

// Select the given row by number.
void SelectAndFocusRowNum(int row, GtkTreeView* tree_view);

// Remove the row and all its children from the |tree_store|.  If there is a
// following row, |iter| will be updated to point to the it and the return value
// will be true, otherwise the return will be false and |iter| is no longer
// valid.
bool RemoveRecursively(GtkTreeStore* tree_store, GtkTreeIter* iter);

// A helper class for populating a GtkListStore from a TableModel.
class TableAdapter : public TableModelObserver {
 public:
  class Delegate {
   public:
    // Should fill in the column and row.
    virtual void SetColumnValues(int row, GtkTreeIter* iter) = 0;

    // Called after any change to the TableModel but before the corresponding
    // change to the GtkListStore.
    virtual void OnAnyModelUpdateStart() {}

    // Called after any change to the TableModel.
    virtual void OnAnyModelUpdate() {}

    // When the TableModel has been completely changed, called by OnModelChanged
    // after clearing the list store.  Can be overriden by the delegate if it
    // needs to do extra initialization before the list store is populated.
    virtual void OnModelChanged() {}
  };

  // |table_model| may be NULL.
  explicit TableAdapter(Delegate* delegate, GtkListStore* list_store,
                        TableModel* table_model);
  virtual ~TableAdapter() {}

  // Replace the TableModel with a different one.  If the list store currenty
  // has items this would cause weirdness, so this should generally only be
  // called during the Delegate's OnModelChanged call, or if the adapter was
  // created with a NULL |table_model|.
  void SetModel(TableModel* table_model);

  // TableModelObserver implementation.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  // Add the values from |row| of the TableModel.
  void AddNodeToList(int row);

  Delegate* delegate_;
  GtkListStore* list_store_;
  TableModel* table_model_;

  DISALLOW_COPY_AND_ASSIGN(TableAdapter);
};

// A helper class for populating a GtkTreeStore from a TreeModel.
// TODO(mattm): support SetRootShown(true)
// TODO(mattm): implement TreeNodeChildrenReordered
class TreeAdapter : public TreeModelObserver {
 public:
  // Column ids for |tree_store_|.
  enum {
    COL_ICON,
    COL_TITLE,
    COL_NODE_PTR,
    COL_COUNT,
  };

  class Delegate {
   public:
    // Called after any change to the TreeModel but before the corresponding
    // change to the GtkTreeStore.
    virtual void OnAnyModelUpdateStart() {}

    // Called after any change to the GtkTreeStore.
    virtual void OnAnyModelUpdate() {}
  };

  TreeAdapter(Delegate* delegate, TreeModel* tree_model);
  virtual ~TreeAdapter();

  // Populate the tree store from the |tree_model_|.
  void Init();

  // Return the tree store.
  GtkTreeStore* tree_store() { return tree_store_; }

  // Get the TreeModelNode corresponding to iter in the tree store.
  TreeModelNode* GetNode(GtkTreeIter* iter);

  // TreeModelObserver implementation.
  virtual void TreeNodesAdded(TreeModel* model,
                              TreeModelNode* parent,
                              int start,
                              int count);
  virtual void TreeNodesRemoved(TreeModel* model,
                                TreeModelNode* parent,
                                int start,
                                int count);
  virtual void TreeNodeChildrenReordered(TreeModel* model,
                                         TreeModelNode* parent);
  virtual void TreeNodeChanged(TreeModel* model, TreeModelNode* node);

 private:
  // Fill the tree store values for a given node.
  void FillRow(GtkTreeIter* iter, TreeModelNode* node);

  // Fill the tree store for a row and all its descendants.
  void Fill(GtkTreeIter* parent_iter, TreeModelNode* parent_node);

  // Get the GtkTreePath in the tree store for the given node.
  // The returned path should be freed with gtk_tree_path_free.
  GtkTreePath* GetTreePath(TreeModelNode* node);

  // Get the GtkTreeIter in the tree store for the given node.
  bool GetTreeIter(TreeModelNode* node, GtkTreeIter* iter);

  Delegate* delegate_;
  GtkTreeStore* tree_store_;
  TreeModel* tree_model_;
  std::vector<GdkPixbuf*> pixbufs_;
};

}  // namespace gtk_tree

#endif  // CHROME_COMMON_GTK_TREE_H_
