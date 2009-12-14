// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include <gtk/gtk.h>

#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/gtk/view_id_util.h"

namespace {

GtkWidget* GetButton(Browser* browser, int index) {
  GtkWidget* button = NULL;
  GtkWidget* toolbar =
      ViewIDUtil::GetWidget(GTK_WIDGET(browser->window()->GetNativeHandle()),
                            VIEW_ID_BROWSER_ACTION_TOOLBAR);
  if (toolbar) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(toolbar));
    button = static_cast<GtkWidget*>(g_list_nth(children, index)->data);
    g_list_free(children);
  }
  return button;
}

}  // namespace

int BrowserActionTestUtil::NumberOfBrowserActions() {
  int count = -1;
  GtkWidget* toolbar =
      ViewIDUtil::GetWidget(GTK_WIDGET(browser_->window()->GetNativeHandle()),
                            VIEW_ID_BROWSER_ACTION_TOOLBAR);
  if (toolbar) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(toolbar));
    count = g_list_length(children);
    g_list_free(children);
  }
  return count;
}

bool BrowserActionTestUtil::HasIcon(int index) {
  GtkWidget* button = GetButton(browser_, index);
  return gtk_button_get_image(GTK_BUTTON(button)) != NULL;
}

void BrowserActionTestUtil::Press(int index) {
  GtkWidget* button = GetButton(browser_, index);
  gtk_button_clicked(GTK_BUTTON(button));
}

std::string BrowserActionTestUtil::GetTooltip(int index) {
  GtkWidget* button = GetButton(browser_, index);
  gchar* text = gtk_widget_get_tooltip_text(button);
  std::string tooltip(text);
  g_free(text);
  return tooltip;
}
