// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/shell_window_gtk.h"

#include "chrome/browser/extensions/extension_host.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_widget_host_view.h"

ShellWindowGtk::ShellWindowGtk(ExtensionHost* host)
    : ShellWindow(host) {
  host_->view()->SetContainer(this);
  window_ = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));

  gtk_container_add(GTK_CONTAINER(window_), host_->view()->native_view());

  const Extension* extension = host_->extension();

  // TOOD(mihaip): restore prior window dimensions and positions on relaunch.
  gtk_window_set_default_size(
      window_, extension->launch_width(), extension->launch_height());

  int min_width = extension->launch_min_width();
  int min_height = extension->launch_min_height();
  int max_width = extension->launch_max_width();
  int max_height = extension->launch_max_height();
  GdkGeometry hints;
  int hints_mask = 0;
  if (min_width || min_height) {
    hints.min_height = min_height;
    hints.min_width = min_width;
    hints_mask |= GDK_HINT_MIN_SIZE;
  }
  if (max_width || max_height) {
    hints.max_height = max_height ? max_height : G_MAXINT;
    hints.max_width = max_width ? max_width : G_MAXINT;
    hints_mask |= GDK_HINT_MAX_SIZE;
  }
  if (hints_mask) {
    gtk_window_set_geometry_hints(
        window_,
        GTK_WIDGET(window_),
        &hints,
        static_cast<GdkWindowHints>(hints_mask));
  }

  // TODO(mihaip): Mirror contents of <title> tag in window title
  gtk_window_set_title(window_, extension->name().c_str());

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
