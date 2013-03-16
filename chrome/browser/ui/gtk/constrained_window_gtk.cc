// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/base/gtk/gtk_hig_constants.h"

namespace {
gboolean OnKeyPress(GtkWidget* sender, GdkEventKey* key, gpointer user_data) {
  if (key->keyval == GDK_Escape) {
    gtk_widget_destroy(sender);
    return TRUE;
  }

  return FALSE;
}
}  // namespace

GtkWidget* CreateWebContentsModalDialogGtk(
    GtkWidget* contents,
    GtkWidget* focus_widget) {
  // Unlike other users of CreateBorderBin, we need a dedicated frame around
  // our "window".
  GtkWidget* border = gtk_event_box_new();
  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
      ui::kContentAreaBorder, ui::kContentAreaBorder,
      ui::kContentAreaBorder, ui::kContentAreaBorder);

  if (gtk_widget_get_parent(contents))
    gtk_widget_reparent(contents, alignment);
  else
    gtk_container_add(GTK_CONTAINER(alignment), contents);

  gtk_container_add(GTK_CONTAINER(frame), alignment);
  gtk_container_add(GTK_CONTAINER(border), frame);

  gtk_widget_add_events(border, GDK_KEY_PRESS_MASK);
  g_signal_connect(border,
                   "key-press-event",
                   reinterpret_cast<GCallback>(&OnKeyPress),
                   NULL);

  // This is a little hacky, but it's better than subclassing GtkWidget just to
  // add one new property.
  g_object_set_data(G_OBJECT(border), "focus_widget", focus_widget);

  return border;
}
