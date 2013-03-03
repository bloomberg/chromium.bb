// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/view_id_util.h"

#include <stdint.h>
#include <string>

#include <gtk/gtk.h>

#include "base/logging.h"

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

const char* GetNameFromID(ViewID id) {
  switch (id) {
    case VIEW_ID_TAB_STRIP:
      return "chrome-tab-strip";

    case VIEW_ID_TOOLBAR:
      return "chrome-toolbar";

    case VIEW_ID_BACK_BUTTON:
      return "chrome-toolbar-back-button";

    case VIEW_ID_FORWARD_BUTTON:
      return "chrome-toolbar-forward-button";

    case VIEW_ID_RELOAD_BUTTON:
      return "chrome-toolbar-reload-button";

    case VIEW_ID_HOME_BUTTON:
      return "chrome-toolbar-home-button";

    case VIEW_ID_STAR_BUTTON:
      return "chrome-toolbar-star-button";

    case VIEW_ID_ACTION_BOX_BUTTON:
      return "chrome-action-box-button";

    case VIEW_ID_BROWSER_ACTION_TOOLBAR:
      return "chrome-toolbar-browser-actions-container";

    case VIEW_ID_OMNIBOX:
      return "chrome-omnibox";

    case VIEW_ID_APP_MENU:
      return "chrome-app-menu";

    case VIEW_ID_BOOKMARK_BAR:
      return "chrome-bookmark-bar";

    case VIEW_ID_OTHER_BOOKMARKS:
      return "chrome-bookmark-bar-other-bookmarks";

    case VIEW_ID_FIND_IN_PAGE_TEXT_FIELD:
      return "chrome-find-in-page-entry";

    case VIEW_ID_FIND_IN_PAGE:
      return "chrome-find-in-page";

    case VIEW_ID_DEV_TOOLS_DOCKED:
      return "chrome-dev-tools-docked";

    case VIEW_ID_ZOOM_BUTTON:
      return "chrome-zoom-button";

    case VIEW_ID_SCRIPT_BUBBLE:
      return "chrome-script-bubble-button";

    // These are never hit because the tab container uses the delegate to
    // set its ID.
    case VIEW_ID_TAB_CONTAINER:
    default:
      NOTREACHED() << "If you added a new VIEW_ID, please provide "
                      "a name for the widget.";
      return NULL;
  }
}

}  // namespace

void ViewIDUtil::SetID(GtkWidget* widget, ViewID id) {
  const char* name = GetNameFromID(id);
  if (name)
    gtk_widget_set_name(widget, name);
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
