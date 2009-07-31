// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gtk_tree_util.h"

#include "base/logging.h"

namespace gtk_tree_util {

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

}  // namespace gtk_tree_util
