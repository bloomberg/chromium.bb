// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_tree_model.h"

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"

namespace {

const char* kCellRendererTextKey = "__CELL_RENDERER_TEXT__";

void AddSingleNodeToTreeStore(GtkTreeStore* store, const BookmarkNode* node,
                              GtkTreeIter *iter, GtkTreeIter* parent) {
  gtk_tree_store_append(store, iter, parent);
  // It would be easy to show a different icon when the folder is open (as they
  // do on Windows, for example), using pixbuf-expander-closed and
  // pixbuf-expander-open. Unfortunately there is no GTK_STOCK_OPEN_DIRECTORY
  // (and indeed, Nautilus does not render an expanded directory any
  // differently).
  gtk_tree_store_set(store, iter,
      bookmark_utils::FOLDER_ICON, GtkThemeService::GetFolderIcon(true),
      bookmark_utils::FOLDER_NAME,
      UTF16ToUTF8(node->GetTitle()).c_str(),
      bookmark_utils::ITEM_ID, node->id(),
      // We don't want to use node->is_folder() because that would let the
      // user edit "Bookmarks Bar" and "Other Bookmarks".
      bookmark_utils::IS_EDITABLE, node->type() == BookmarkNode::FOLDER,
      -1);
}

// Helper function for CommitTreeStoreDifferencesBetween() which recursively
// merges changes back from a GtkTreeStore into a tree of BookmarkNodes. This
// function only works on non-root nodes; our caller handles that special case.
void RecursiveResolve(BookmarkModel* bb_model,
                      const BookmarkNode* bb_node,
                      GtkTreeStore* tree_store,
                      GtkTreeIter* parent_iter,
                      GtkTreePath* selected_path,
                      const BookmarkNode** selected_node) {
  GtkTreePath* current_path =
      gtk_tree_model_get_path(GTK_TREE_MODEL(tree_store), parent_iter);
  if (gtk_tree_path_compare(current_path, selected_path) == 0)
    *selected_node = bb_node;
  gtk_tree_path_free(current_path);

  GtkTreeIter child_iter;
  if (gtk_tree_model_iter_children(GTK_TREE_MODEL(tree_store), &child_iter,
                                   parent_iter)) {
    do {
      int64 id = bookmark_utils::GetIdFromTreeIter(GTK_TREE_MODEL(tree_store),
                                                   &child_iter);
      string16 title =
          bookmark_utils::GetTitleFromTreeIter(GTK_TREE_MODEL(tree_store),
                                               &child_iter);
      const BookmarkNode* child_bb_node = NULL;
      if (id == 0) {
        child_bb_node = bb_model->AddFolder(
            bb_node, bb_node->child_count(), title);

        // Set the value in the model so if we lookup the id later we get the
        // real id and not 0.
        GValue value  = { 0 };
        g_value_init(&value, G_TYPE_INT64);
        g_value_set_int64(&value, child_bb_node->id());
        gtk_tree_store_set_value(tree_store, &child_iter,
                                 bookmark_utils::ITEM_ID, &value);
      } else {
        // Existing node, reset the title (BookmarkModel ignores changes if the
        // title is the same).
        for (int j = 0; j < bb_node->child_count(); ++j) {
          const BookmarkNode* node = bb_node->GetChild(j);
          if (node->is_folder() && node->id() == id) {
            child_bb_node = node;
            break;
          }
        }
        DCHECK(child_bb_node);
        bb_model->SetTitle(child_bb_node, title);
      }
      RecursiveResolve(bb_model, child_bb_node, tree_store, &child_iter,
                       selected_path, selected_node);
    } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(tree_store), &child_iter));
  }
}

// Update the folder name in the GtkTreeStore.
void OnFolderNameEdited(GtkCellRendererText* render,
    gchar* path, gchar* new_folder_name, GtkTreeStore* tree_store) {
  GtkTreeIter folder_iter;
  GtkTreePath* tree_path = gtk_tree_path_new_from_string(path);
  gboolean rv = gtk_tree_model_get_iter(GTK_TREE_MODEL(tree_store),
                                        &folder_iter, tree_path);
  DCHECK(rv);
  gtk_tree_store_set(tree_store, &folder_iter,
                     bookmark_utils::FOLDER_NAME, new_folder_name,
                     -1);
  gtk_tree_path_free(tree_path);
}

}  // namespace

namespace bookmark_utils {

GtkTreeStore* MakeFolderTreeStore() {
  return gtk_tree_store_new(FOLDER_STORE_NUM_COLUMNS, GDK_TYPE_PIXBUF,
                            G_TYPE_STRING, G_TYPE_INT64, G_TYPE_BOOLEAN);
}

void AddToTreeStore(BookmarkModel* model, int64 selected_id,
                    GtkTreeStore* store, GtkTreeIter* selected_iter) {
  const BookmarkNode* root_node = model->root_node();
  for (int i = 0; i < root_node->child_count(); ++i) {
    const BookmarkNode* child = root_node->GetChild(i);
    AddToTreeStoreAt(child, selected_id, store, selected_iter, NULL);
  }
}

GtkWidget* MakeTreeViewForStore(GtkTreeStore* store) {
  GtkTreeViewColumn* column = gtk_tree_view_column_new();
  GtkCellRenderer* image_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, image_renderer, FALSE);
  gtk_tree_view_column_add_attribute(column, image_renderer,
                                     "pixbuf", FOLDER_ICON);
  GtkCellRenderer* text_renderer = gtk_cell_renderer_text_new();
  g_object_set(text_renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
  g_signal_connect(text_renderer, "edited", G_CALLBACK(OnFolderNameEdited),
                   store);
  gtk_tree_view_column_pack_start(column, text_renderer, TRUE);
  gtk_tree_view_column_set_attributes(column, text_renderer,
                                      "text", FOLDER_NAME,
                                      "editable", IS_EDITABLE,
                                      NULL);

  GtkWidget* tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  // Let |tree_view| own the store.
  g_object_unref(store);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), FALSE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
  g_object_set_data(G_OBJECT(tree_view), kCellRendererTextKey, text_renderer);
  return tree_view;
}

GtkCellRenderer* GetCellRendererText(GtkTreeView* tree_view) {
  return static_cast<GtkCellRenderer*>(
      g_object_get_data(G_OBJECT(tree_view), kCellRendererTextKey));
}

void AddToTreeStoreAt(const BookmarkNode* node, int64 selected_id,
                      GtkTreeStore* store, GtkTreeIter* selected_iter,
                      GtkTreeIter* parent) {
  if (!node->is_folder())
    return;

  GtkTreeIter iter;
  AddSingleNodeToTreeStore(store, node, &iter, parent);
  if (selected_iter && node->id() == selected_id) {
     // Save the iterator. Since we're using a GtkTreeStore, we're
     // guaranteed that the iterator will remain valid as long as the above
     // appended item exists.
     *selected_iter = iter;
  }

  for (int i = 0; i < node->child_count(); ++i) {
    AddToTreeStoreAt(node->GetChild(i), selected_id, store, selected_iter,
                     &iter);
  }
}

const BookmarkNode* CommitTreeStoreDifferencesBetween(
    BookmarkModel* bb_model, GtkTreeStore* tree_store, GtkTreeIter* selected) {
  const BookmarkNode* node_to_return = NULL;
  GtkTreeModel* tree_model = GTK_TREE_MODEL(tree_store);

  GtkTreePath* selected_path = gtk_tree_model_get_path(tree_model, selected);

  GtkTreeIter tree_root;
  if (!gtk_tree_model_get_iter_first(tree_model, &tree_root))
    NOTREACHED() << "Impossible missing bookmarks case";

  // The top level of this tree is weird and needs to be special cased. The
  // BookmarksNode tree is rooted on a root node while the GtkTreeStore has a
  // set of top level nodes that are the root BookmarksNode's children. These
  // items in the top level are not editable and therefore don't need the extra
  // complexity of trying to modify their title.
  const BookmarkNode* root_node = bb_model->root_node();
  do {
    DCHECK(GetIdFromTreeIter(tree_model, &tree_root) != 0)
        << "It should be impossible to add another toplevel node";

    int64 id = GetIdFromTreeIter(tree_model, &tree_root);
    const BookmarkNode* child_node = NULL;
    for (int j = 0; j < root_node->child_count(); ++j) {
      const BookmarkNode* node = root_node->GetChild(j);
      if (node->is_folder() && node->id() == id) {
        child_node = node;
        break;
      }
    }
    DCHECK(child_node);

    GtkTreeIter child_iter = tree_root;
    RecursiveResolve(bb_model, child_node, tree_store, &child_iter,
                     selected_path, &node_to_return);
  } while (gtk_tree_model_iter_next(tree_model, &tree_root));

  gtk_tree_path_free(selected_path);
  return node_to_return;
}

int64 GetIdFromTreeIter(GtkTreeModel* model, GtkTreeIter* iter) {
  GValue value = { 0, };
  int64 ret_val = -1;
  gtk_tree_model_get_value(model, iter, ITEM_ID, &value);
  if (G_VALUE_HOLDS_INT64(&value))
    ret_val = g_value_get_int64(&value);
  else
    NOTREACHED() << "Impossible type mismatch";

  return ret_val;
}

string16 GetTitleFromTreeIter(GtkTreeModel* model, GtkTreeIter* iter) {
  GValue value = { 0, };
  string16 ret_val;
  gtk_tree_model_get_value(model, iter, FOLDER_NAME, &value);
  if (G_VALUE_HOLDS_STRING(&value)) {
    const gchar* utf8str = g_value_get_string(&value);
    ret_val = UTF8ToUTF16(utf8str);
    g_value_unset(&value);
  } else {
    NOTREACHED() << "Impossible type mismatch";
  }

  return ret_val;
}

}  // namespace bookmark_utils
