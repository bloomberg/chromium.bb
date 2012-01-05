// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autofill_popup_view_gtk.h"

#include "base/logging.h"
#include "ui/base/gtk/gtk_windowing.h"

AutofillPopupViewGtk::AutofillPopupViewGtk(content::WebContents* web_contents,
                                           GtkWidget* parent)
    : AutofillPopupView(web_contents),
      parent_(parent),
      window_(gtk_window_new(GTK_WINDOW_POPUP)) {
  CHECK(parent != NULL);
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_app_paintable(window_, TRUE);
  gtk_widget_set_double_buffered(window_, TRUE);

  // Setup the window to ensure it recieves the expose event.
  gtk_widget_add_events(window_, GDK_EXPOSURE_MASK);
  g_signal_connect(window_, "expose-event",
                   G_CALLBACK(HandleExposeThunk), this);
}

AutofillPopupViewGtk::~AutofillPopupViewGtk() {
  gtk_widget_destroy(window_);
}

void AutofillPopupViewGtk::Hide() {
  gtk_widget_hide(window_);
}

// TODO(csharp): Actually show the values.
void AutofillPopupViewGtk::Show(const std::vector<string16>& autofill_values,
                                const std::vector<string16>& autofill_labels,
                                const std::vector<string16>& autofill_icons,
                                const std::vector<int>& autofill_unique_ids,
                                int separator_index) {
  gint origin_x, origin_y;
  gdk_window_get_origin(gtk_widget_get_window(parent_), &origin_x, &origin_y);

  gtk_window_move(GTK_WINDOW(window_),
                  origin_x + element_bounds().x(),
                  origin_y + element_bounds().y() + element_bounds().height());

  gtk_widget_set_size_request(
      window_,
      element_bounds().width(),
      element_bounds().height() * autofill_values.size());

  gtk_widget_show(window_);

  GtkWidget* toplevel = gtk_widget_get_toplevel(parent_);
  CHECK(gtk_widget_is_toplevel(toplevel));
  ui::StackPopupWindow(window_, toplevel);
}

gboolean AutofillPopupViewGtk::HandleExpose(GtkWidget* widget,
                                            GdkEventExpose* event) {
  return TRUE;
}
