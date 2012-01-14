// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string_piece.h"
#include "content/public/browser/browser_context.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view_gtk.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

// Callback for when the main window is destroyed.
gboolean MainWindowDestroyed(GtkWindow* window) {
  // TODO(erg): Only shut down when there are multiple windows.
  MessageLoop::current()->PostTask(FROM_HERE,
                                   MessageLoop::QuitClosure());

  return FALSE;  // Don't stop this message.
}

}  // namespace

namespace content {

void Shell::PlatformInitialize() {
  gtk_init(NULL, NULL);
}

base::StringPiece Shell::PlatformResourceProvider(int key) {
  NOTIMPLEMENTED();
  return base::StringPiece();
}

void Shell::PlatformCleanUp() {
  NOTIMPLEMENTED();
}

void Shell::PlatformEnableUIControl(UIControl control, bool is_enabled) {
  GtkToolItem* item = NULL;
  switch (control) {
    case BACK_BUTTON:
      item = back_button_;
      break;
    case FORWARD_BUTTON:
      item = forward_button_;
      break;
    case STOP_BUTTON:
      item = stop_button_;
      break;
    default:
      NOTREACHED() << "Unknown UI control";
      return;
  }
  gtk_widget_set_sensitive(GTK_WIDGET(item), is_enabled);
}

void Shell::PlatformSetAddressBarURL(const GURL& url) {
}

void Shell::PlatformCreateWindow(int width, int height) {
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gtk_window_set_title(window_, "Content Shell");
  // Null out window_ when it is destroyed so we don't destroy it twice.
  g_signal_connect(G_OBJECT(window_), "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &window_);
  g_signal_connect(G_OBJECT(window_), "destroy",
                   G_CALLBACK(MainWindowDestroyed), this);

  // TODO(erg): The test shell closes popup windows when the main window goes
  // out.
  // g_signal_connect(G_OBJECT(window_), "focus-out-event",
  //                  G_CALLBACK(MainWindowLostFocus), this);

  vbox_ = gtk_vbox_new(FALSE, 0);

  GtkWidget* toolbar = gtk_toolbar_new();
  // Turn off the labels on the toolbar buttons.
  gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

  back_button_ = gtk_tool_button_new_from_stock(GTK_STOCK_GO_BACK);
  g_signal_connect(back_button_, "clicked",
                   G_CALLBACK(&OnBackButtonClickedThunk), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), back_button_, -1 /* append */);

  forward_button_ = gtk_tool_button_new_from_stock(GTK_STOCK_GO_FORWARD);
  g_signal_connect(forward_button_, "clicked",
                   G_CALLBACK(&OnForwardButtonClickedThunk), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), forward_button_, -1 /* append */);

  reload_button_ = gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH);
  g_signal_connect(reload_button_, "clicked",
                   G_CALLBACK(&OnReloadButtonClickedThunk), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), reload_button_, -1 /* append */);

  stop_button_ = gtk_tool_button_new_from_stock(GTK_STOCK_STOP);
  g_signal_connect(stop_button_, "clicked",
                   G_CALLBACK(&OnStopButtonClickedThunk), this);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), stop_button_, -1 /* append */);

  url_edit_view_ = gtk_entry_new();
  g_signal_connect(G_OBJECT(url_edit_view_), "activate",
                   G_CALLBACK(&OnURLEntryActivateThunk), this);
  // gtk_entry_set_text(GTK_ENTRY(url_edit_view_), starting_url.spec().c_str());

  GtkToolItem* tool_item = gtk_tool_item_new();
  gtk_container_add(GTK_CONTAINER(tool_item), url_edit_view_);
  gtk_tool_item_set_expand(tool_item, TRUE);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), tool_item, -1 /* append */);

  gtk_box_pack_start(GTK_BOX(vbox_), toolbar, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(window_), vbox_);
  gtk_widget_show_all(GTK_WIDGET(window_));

  PlatformSizeTo(width, height);
}

void Shell::PlatformSetContents() {
  TabContentsViewGtk* content_view =
      static_cast<TabContentsViewGtk*>(tab_contents_->GetView());
  gtk_container_add(GTK_CONTAINER(vbox_), content_view->GetNativeView());

  // As an additional requirement on Linux, we must set the colors for the
  // render widgets in webkit.
  content::RendererPreferences* prefs =
      tab_contents_->GetMutableRendererPrefs();
  prefs->focus_ring_color = SkColorSetARGB(255, 229, 151, 0);
  prefs->thumb_active_color = SkColorSetRGB(244, 244, 244);
  prefs->thumb_inactive_color = SkColorSetRGB(234, 234, 234);
  prefs->track_color = SkColorSetRGB(211, 211, 211);

  prefs->active_selection_bg_color = SkColorSetRGB(30, 144, 255);
  prefs->active_selection_fg_color = SK_ColorWHITE;
  prefs->inactive_selection_bg_color = SkColorSetRGB(200, 200, 200);
  prefs->inactive_selection_fg_color = SkColorSetRGB(50, 50, 50);
}

void Shell::PlatformSizeTo(int width, int height) {
  content_width_ = width;
  content_height_ = height;
  if (tab_contents_.get()) {
    gtk_widget_set_size_request(tab_contents_->GetNativeView(), width, height);
  }
}

void Shell::PlatformResizeSubViews() {
  PlatformSizeTo(content_width_, content_height_);
}

void Shell::OnBackButtonClicked(GtkWidget* widget) {
  GoBackOrForward(-1);
}

void Shell::OnForwardButtonClicked(GtkWidget* widget) {
  GoBackOrForward(1);
}

void Shell::OnReloadButtonClicked(GtkWidget* widget) {
  Reload();
}

void Shell::OnStopButtonClicked(GtkWidget* widget) {
  Stop();
}

void Shell::OnURLEntryActivate(GtkWidget* entry) {
  const gchar* url = gtk_entry_get_text(GTK_ENTRY(entry));
  LoadURL(GURL(url));
}

}  // namespace content
