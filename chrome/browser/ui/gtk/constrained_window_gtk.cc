// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tab_contents/chrome_web_contents_view_delegate_gtk.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/gtk/focus_store_gtk.h"
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

ConstrainedWindowGtk::ConstrainedWindowGtk(
    GtkWidget* contents,
    GtkWidget* focus_widget) {
  // Unlike other users of CreateBorderBin, we need a dedicated frame around
  // our "window".
  border_ = gtk_event_box_new();
  g_object_ref_sink(border_);
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
  gtk_container_add(GTK_CONTAINER(border_), frame);

  gtk_widget_add_events(border_, GDK_KEY_PRESS_MASK);
  g_signal_connect(border_,
                   "key-press-event",
                   reinterpret_cast<GCallback>(&OnKeyPress),
                   NULL);

  // This is a little hacky, but it's better than subclassing GtkWidget just to
  // add one new property.
  g_object_set_data(G_OBJECT(border_), "focus_widget", focus_widget);

  // TODO(wittman): Getting/setting data on the widget is a hack to facilitate
  // looking up the ConstrainedWindowGtk from the GtkWindow during refactoring.
  // Remove once ConstrainedWindowGtk is gone.
  g_object_set_data(G_OBJECT(border_), "ConstrainedWindowGtk", this);
}

ConstrainedWindowGtk::~ConstrainedWindowGtk() {
  g_object_unref(border_);
  border_ = NULL;
}

GtkWidget* CreateWebContentsModalDialogGtk(
    GtkWidget* contents,
    GtkWidget* focus_widget) {
  ConstrainedWindowGtk* window =
      new ConstrainedWindowGtk(contents, focus_widget);
  return window->widget();
}
