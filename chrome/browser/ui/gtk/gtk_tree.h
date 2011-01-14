// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_TREE_H_
#define CHROME_BROWSER_UI_GTK_GTK_TREE_H_
#pragma once

#include <gtk/gtk.h>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "chrome/browser/remove_rows_table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/base/models/tree_model.h"

namespace ui {
class TableModel;
}

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

// Writes all the indexes of selected rows into |out|.
void GetSelectedIndices(GtkTreeSelection* selection, std::set<int>* out);

// A helper class for populating a GtkListStore from a ui::TableModel.
class TableAdapter : public ui::TableModelObserver {
 public:

  enum ColumnID {
    COL_TITLE = 0,
    COL_IS_HEADER,
    COL_IS_SEPARATOR,
    COL_GROUP_ID,
    COL_WEIGHT,
    COL_WEIGHT_SET,
    COL_LAST_ID
  };

  class Delegate {
   public:
    // Should fill in the column and row.
    virtual void SetColumnValues(int row, GtkTreeIter* iter) = 0;

    // Called after any change to the ui::TableModel but before the
    // corresponding change to the GtkListStore.
    virtual void OnAnyModelUpdateStart() {}

    // Called after any change to the ui::TableModel.
    virtual void OnAnyModelUpdate() {}

    // When the ui::TableModel has been completely changed, called by
    // OnModelChanged after clearing the list store.  Can be overridden by the
    // delegate if it needs to do extra initialization before the list store is
    // populated.
    virtual void OnModelChanged() {}

   protected:
    virtual ~Delegate() {}
  };

  // |table_model| may be NULL.
  TableAdapter(Delegate* delegate,
               GtkListStore* list_store,
               ui::TableModel* table_model);
  virtual ~TableAdapter() {}

  // Replace the ui::TableModel with a different one.  If the list store
  // currently has items this would cause weirdness, so this should generally
  // only be called during the Delegate's OnModelChanged call, or if the adapter
  // was created with a NULL |table_model|.
  void SetModel(ui::TableModel* table_model);

  // Add all model rows corresponding to the given list store indices to |rows|.
  void MapListStoreIndicesToModelRows(const std::set<int>& list_store_indices,
                                      RemoveRowsTableModel::Rows* model_rows);

  // GtkTreeModel callbacks:
  // Callback checking whether a row should be drawn as a separator.
  static gboolean OnCheckRowIsSeparator(GtkTreeModel* model,
                                        GtkTreeIter* iter,
                                        gpointer user_data);

  // Callback checking whether a row may be selected.  We use some rows in the
  // table as headers/separators for the groups, which should not be selectable.
  static gboolean OnSelectionFilter(GtkTreeSelection* selection,
                                    GtkTreeModel* model,
                                    GtkTreePath* path,
                                    gboolean path_currently_selected,
                                    gpointer user_data);

  // ui::TableModelObserver implementation.
  virtual void OnModelChanged();
  virtual void OnItemsChanged(int start, int length);
  virtual void OnItemsAdded(int start, int length);
  virtual void OnItemsRemoved(int start, int length);

 private:
  // Return whether the row pointed to by |iter| is a group row, i.e. a group
  // header, or a separator.
  bool IsGroupRow(GtkTreeIter* iter) const;

  // Return the index into the list store for the given model row.
  int GetListStoreIndexForModelRow(int model_row) const;

  // Add the values from |row| of the ui::TableModel.
  void AddNodeToList(int row);

  Delegate* delegate_;
  GtkListStore* list_store_;
  ui::TableModel* table_model_;

  DISALLOW_COPY_AND_ASSIGN(TableAdapter);
};

// A helper class for populating a GtkTreeStore from a TreeModel.
// TODO(mattm): support SetRootShown(true)
class TreeAdapter : public ui::TreeModelObserver {
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

   protected:
    virtual ~Delegate() {}
  };

  TreeAdapter(Delegate* delegate, ui::TreeModel* tree_model);
  virtual ~TreeAdapter();

  // Populate the tree store from the |tree_model_|.
  void Init();

  // Return the tree store.
  GtkTreeStore* tree_store() { return tree_store_; }

  // Get the TreeModelNode corresponding to iter in the tree store.
  ui::TreeModelNode* GetNode(GtkTreeIter* iter);

  // Begin TreeModelObserver implementation.
  virtual void TreeNodesAdded(ui::TreeModel* model,
                              ui::TreeModelNode* parent,
                              int start,
                              int count);
  virtual void TreeNodesRemoved(ui::TreeModel* model,
                                ui::TreeModelNode* parent,
                                int start,
                                int count);
  virtual void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node);
  // End TreeModelObserver implementation.

 private:
  // Fill the tree store values for a given node.
  void FillRow(GtkTreeIter* iter, ui::TreeModelNode* node);

  // Fill the tree store for a row and all its descendants.
  void Fill(GtkTreeIter* parent_iter, ui::TreeModelNode* parent_node);

  // Get the GtkTreePath in the tree store for the given node.
  // The returned path should be freed with gtk_tree_path_free.
  GtkTreePath* GetTreePath(ui::TreeModelNode* node);

  // Get the GtkTreeIter in the tree store for the given node.
  bool GetTreeIter(ui::TreeModelNode* node, GtkTreeIter* iter);

  Delegate* delegate_;
  GtkTreeStore* tree_store_;
  ui::TreeModel* tree_model_;
  std::vector<GdkPixbuf*> pixbufs_;

  DISALLOW_COPY_AND_ASSIGN(TreeAdapter);
};

}  // namespace gtk_tree

#endif  // CHROME_BROWSER_UI_GTK_GTK_TREE_H_
