// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_GTK_TREE_UTIL_H_
#define CHROME_COMMON_GTK_TREE_UTIL_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"

class GtkTreeUtil {
 public:
  // Get the row number corresponding to |path|.
  static gint GetRowNumForPath(GtkTreePath* path);

  // Get the row number corresponding to |iter|.
  static gint GetRowNumForIter(GtkTreeModel* model, GtkTreeIter* iter);

  // Get the row number in the child tree model corresponding to |sort_path| in
  // the parent tree model.
  static gint GetTreeSortChildRowNumForPath(GtkTreeModel* sort_model,
                                            GtkTreePath* sort_path);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(GtkTreeUtil);
};

#endif  // CHROME_COMMON_GTK_TREE_UTIL_H_
