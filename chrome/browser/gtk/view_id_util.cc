// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/view_id_util.h"

#include <stdint.h>

#include <gtk/gtk.h>

namespace {

const char kViewIDString[] = "__VIEW_ID__";
const char kViewIDOverrideString[] = "__VIEW_ID_OVERRIDE__";

struct ViewIDSearchStruct {
  ViewID id;  // Input: the ID we are searching for.
  GtkWidget* widget;  // Output: the found widget, or NULL.
};

// Recursively search for the given view ID among the children of |widget|.
void SearchForWidgetWithViewID(GtkWidget* widget, gpointer data) {
  ViewIDSearchStruct* search_struct =
      reinterpret_cast<ViewIDSearchStruct*>(data);

  // The widget has already been found; abort the search.
  if (search_struct->widget)
    return;

  // Check if the widget defines its own ID function.
  ViewIDUtil::Delegate* delegate = reinterpret_cast<ViewIDUtil::Delegate*>(
      g_object_get_data(G_OBJECT(widget), kViewIDOverrideString));
  if (delegate) {
    search_struct->widget = delegate->GetWidgetForViewID(search_struct->id);
    // If there was success, return.
    if (search_struct->widget)
      return;
  }

  // Otherwise check the g_object data.
  int widget_id =
      reinterpret_cast<intptr_t>(g_object_get_data(G_OBJECT(widget),
                                 kViewIDString));
  if (search_struct->id == widget_id) {
    // Success; set |widget| and return.
    search_struct->widget = widget;
    return;
  }

  // Recurse.
  if (GTK_IS_CONTAINER(widget)) {
    gtk_container_foreach(GTK_CONTAINER(widget),
                          SearchForWidgetWithViewID, data);
  }
}

}  // namespace

void ViewIDUtil::SetID(GtkWidget* widget, ViewID id) {
  g_object_set_data(G_OBJECT(widget), kViewIDString,
                    reinterpret_cast<void*>(id));
}

GtkWidget* ViewIDUtil::GetWidget(GtkWidget* root, ViewID id) {
  ViewIDSearchStruct search_struct = { id, NULL };
  SearchForWidgetWithViewID(root, &search_struct);
  return search_struct.widget;
}

void ViewIDUtil::SetDelegateForWidget(GtkWidget* widget, Delegate* delegate) {
  g_object_set_data(G_OBJECT(widget), kViewIDOverrideString, delegate);
}
