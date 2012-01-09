// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/shell_window_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "content/browser/renderer_host/render_widget_host_view_gtk.h"

ShellWindowGtk::ShellWindowGtk(ExtensionHost* host)
    : ShellWindow(host) {
  host_->view()->SetContainer(this);
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

  gtk_container_add(GTK_CONTAINER(window_), host_->view()->native_view());

  // TOOD(mihaip): Allow window dimensions to be specified in manifest (and
  // restore prior window dimensions and positions on relaunch).
  gtk_widget_set_size_request(GTK_WIDGET(window_), 512, 384);
  gtk_window_set_decorated(window_, false);

  g_signal_connect(window_, "delete-event",
                   G_CALLBACK(OnMainWindowDeleteEventThunk), this);

  gtk_window_present(window_);
}

ShellWindowGtk::~ShellWindowGtk() {
}

void ShellWindowGtk::Close() {
  gtk_widget_destroy(GTK_WIDGET(window_));
  delete this;
}

// Callback for the delete event.  This event is fired when the user tries to
// close the window (e.g., clicking on the X in the window manager title bar).
gboolean ShellWindowGtk::OnMainWindowDeleteEvent(GtkWidget* widget,
                                                 GdkEvent* event) {
  Close();

  // Return true to prevent the GTK window from being destroyed.  Close will
  // destroy it for us.
  return TRUE;
}

// static
ShellWindow* ShellWindow::CreateShellWindow(ExtensionHost* host) {
  return new ShellWindowGtk(host);
}
