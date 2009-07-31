// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GTK_TREE_UTIL_H_
#define CHROME_COMMON_GTK_TREE_UTIL_H_

#include <gtk/gtk.h>

namespace gtk_tree_util {

// Get the row number corresponding to |path|.
gint GetRowNumForPath(GtkTreePath* path);

// Get the row number corresponding to |iter|.
gint GetRowNumForIter(GtkTreeModel* model, GtkTreeIter* iter);

// Get the row number in the child tree model corresponding to |sort_path| in
// the parent tree model.
gint GetTreeSortChildRowNumForPath(GtkTreeModel* sort_model,
                                   GtkTreePath* sort_path);

}  // namespace gtk_tree_util

#endif  // CHROME_COMMON_GTK_TREE_UTIL_H_
