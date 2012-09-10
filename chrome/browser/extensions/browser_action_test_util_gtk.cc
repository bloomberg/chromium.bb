// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"

#include <gtk/gtk.h>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/extensions/extension_popup_gtk.h"
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#include "chrome/browser/ui/gtk/view_id_util.h"
#include "ui/gfx/image/image.h"

namespace {

GtkWidget* GetButton(Browser* browser, int index) {
  GtkWidget* toolbar =
      ViewIDUtil::GetWidget(GTK_WIDGET(browser->window()->GetNativeWindow()),
                            VIEW_ID_BROWSER_ACTION_TOOLBAR);
  GtkWidget* button = NULL;
  if (toolbar) {
    GList* children = gtk_container_get_children(GTK_CONTAINER(toolbar));
    GtkWidget* alignment =
        static_cast<GtkWidget*>(g_list_nth(children, index)->data);
    button = gtk_bin_get_child(GTK_BIN(alignment));
    g_list_free(children);
  }
  return button;
}

}  // namespace

int BrowserActionTestUtil::NumberOfBrowserActions() {
  int count = -1;
  GtkWidget* toolbar =
      ViewIDUtil::GetWidget(GTK_WIDGET(browser_->window()->GetNativeWindow()),
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

gfx::Image BrowserActionTestUtil::GetIcon(int index) {
  GtkWidget* button = GetButton(browser_, index);
  GtkWidget* image = gtk_button_get_image(GTK_BUTTON(button));
  GdkPixbuf* pixbuf = gtk_image_get_pixbuf(GTK_IMAGE(image));
  // gfx::Image takes ownership of the |pixbuf| reference. We have to increase
  // the ref count so |pixbuf| stays around when the image object is destroyed.
  g_object_ref(pixbuf);
  return gfx::Image(pixbuf);
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

bool BrowserActionTestUtil::HasPopup() {
  return ExtensionPopupGtk::get_current_extension_popup() != NULL;
}

gfx::Rect BrowserActionTestUtil::GetPopupBounds() {
  ExtensionPopupGtk* popup = ExtensionPopupGtk::get_current_extension_popup();
  if (popup)
    return popup->GetViewBounds();
  return gfx::Rect();
}

bool BrowserActionTestUtil::HidePopup() {
  ExtensionPopupGtk* popup = ExtensionPopupGtk::get_current_extension_popup();
  if (popup)
    return popup->DestroyPopup();
  return false;
}

// static
gfx::Size BrowserActionTestUtil::GetMinPopupSize() {
  // On Linux we actually just limit the size of the extension view.
  return gfx::Size(ExtensionPopupGtk::kMinWidth, ExtensionPopupGtk::kMinHeight);
}

// static
gfx::Size BrowserActionTestUtil::GetMaxPopupSize() {
  return gfx::Size(ExtensionPopupGtk::kMaxWidth, ExtensionPopupGtk::kMaxHeight);
}
