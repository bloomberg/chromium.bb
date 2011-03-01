// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_TREE_MODEL_H_
#define CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_TREE_MODEL_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"

class BookmarkModel;
class BookmarkNode;

typedef struct _GtkCellRenderer GtkCellRenderer;
typedef struct _GtkTreeIter GtkTreeIter;
typedef struct _GtkTreeModel GtkTreeModel;
typedef struct _GtkTreeStore GtkTreeStore;
typedef struct _GtkTreeView GtkTreeView;
typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GtkWidget GtkWidget;

namespace bookmark_utils {

enum FolderTreeStoreColumns {
  FOLDER_ICON,
  FOLDER_NAME,
  ITEM_ID,
  IS_EDITABLE,
  FOLDER_STORE_NUM_COLUMNS
};

// Make a tree store that has two columns: name and id.
GtkTreeStore* MakeFolderTreeStore();

// Copies the folders in the model's root node into a GtkTreeStore. We
// want the user to be able to modify the tree of folders, but to be able to
// click Cancel and discard their modifications. |selected_id| is the
// node->id() of the BookmarkNode that should selected on
// node->screen. |selected_iter| is an out value that points to the
// node->representation of the node associated with |selected_id| in |store|.
// |recursive| indicates whether to recurse into sub-directories (if false,
// the tree store will effectively be a list). |only_folders| indicates whether
// to include bookmarks in the tree, or to only show folders.
void AddToTreeStore(BookmarkModel* model, int64 selected_id,
                    GtkTreeStore* store, GtkTreeIter* selected_iter);

// As above, but inserts just the tree rooted at |node| as a child of |parent|.
// If |parent| is NULL, add it at the top level.
void AddToTreeStoreAt(const BookmarkNode* node, int64 selected_id,
                      GtkTreeStore* store, GtkTreeIter* selected_iter,
                      GtkTreeIter* parent);

// Makes a tree view for the store. This will take ownership of |store| and the
// returned widget has a floating reference.
GtkWidget* MakeTreeViewForStore(GtkTreeStore* store);

// A helper method for getting pointer back to the GtkCellRendererText used for
// the folder names.
GtkCellRenderer* GetCellRendererText(GtkTreeView* tree_view);

// Commits changes to a GtkTreeStore built from BuildTreeStoreFrom() back
// into the BookmarkModel it was generated from.  Returns the BookmarkNode that
// represented by |selected|.
const BookmarkNode* CommitTreeStoreDifferencesBetween(
    BookmarkModel* model, GtkTreeStore* tree_store,
    GtkTreeIter* selected);

// Returns the id field of the row pointed to by |iter|.
int64 GetIdFromTreeIter(GtkTreeModel* model, GtkTreeIter* iter);

// Returns the title field in utf8 of the row pointed to by |iter|.
string16 GetTitleFromTreeIter(GtkTreeModel* model, GtkTreeIter* iter);

}  // namespace bookmark_utils

#endif  // CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_TREE_MODEL_H_
