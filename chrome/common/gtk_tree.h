// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GTK_TREE_H_
#define CHROME_COMMON_GTK_TREE_H_

#include <gtk/gtk.h>

#include "app/table_model_observer.h"
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

// A helper class for populating a GtkListStore from a TableModel.
class TableAdapter : public TableModelObserver {
 public:
  class Delegate {
   public:
    // Should fill in the column and row.
    virtual void SetColumnValues(int row, GtkTreeIter* iter) = 0;

    // Called before any change to the TableModel.  Overriding optional.
    virtual void OnAnyModelUpdateStart() {}

    // Called after any change to the TableModel.  Overriding optional.
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

}  // namespace gtk_tree

#endif  // CHROME_COMMON_GTK_TREE_H_
