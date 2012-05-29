// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/web_auth_flow_window_gtk.h"

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/gfx/native_widget_types.h"

using content::BrowserContext;
using content::WebContents;

WebAuthFlowWindowGtk::WebAuthFlowWindowGtk(
    Delegate* delegate,
    BrowserContext* browser_context,
    WebContents* contents)
    : WebAuthFlowWindow(delegate, browser_context, contents),
      window_(NULL) {
}

void WebAuthFlowWindowGtk::Show() {
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
  gfx::NativeView native_view = contents()->GetView()->GetNativeView();
  gtk_container_add(GTK_CONTAINER(window_), native_view);
  gtk_window_set_default_size(window_, kDefaultWidth, kDefaultHeight);

  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(OnMainWindowDeleteEventThunk), this);

  gtk_window_present(window_);
}

WebAuthFlowWindowGtk::~WebAuthFlowWindowGtk() {
  if (window_)
    gtk_widget_destroy(GTK_WIDGET(window_));
}

// Callback for the delete event.  This event is fired when the user tries to
// close the window (e.g., clicking on the X in the window manager title bar).
gboolean WebAuthFlowWindowGtk::OnMainWindowDeleteEvent(
    GtkWidget* widget, GdkEvent* event) {
  if (delegate())
    delegate()->OnClose();

  // Return FALSE to tell the caller to delete the window.
  return FALSE;
}

// static
WebAuthFlowWindow* WebAuthFlowWindow::Create(
    Delegate* delegate,
    BrowserContext* browser_context,
    WebContents* contents) {
  return new WebAuthFlowWindowGtk(delegate, browser_context, contents);
}
