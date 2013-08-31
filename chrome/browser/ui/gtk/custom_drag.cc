// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/custom_drag.h"

#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/gfx/image/image.h"

CustomDrag::CustomDrag(gfx::Image* icon, int code_mask, GdkDragAction action)
    : drag_widget_(gtk_invisible_new()),
      image_(icon) {
  g_signal_connect(drag_widget_, "drag-data-get",
                   G_CALLBACK(OnDragDataGetThunk), this);
  g_signal_connect(drag_widget_, "drag-begin",
                   G_CALLBACK(OnDragBeginThunk), this);
  g_signal_connect(drag_widget_, "drag-end",
                   G_CALLBACK(OnDragEndThunk), this);

  GtkTargetList* list = ui::GetTargetListFromCodeMask(code_mask);
  GdkEvent* event = gtk_get_current_event();
  gtk_drag_begin(drag_widget_, list, action, 1, event);
  if (event)
    gdk_event_free(event);
  gtk_target_list_unref(list);
}

CustomDrag::~CustomDrag() {
  gtk_widget_destroy(drag_widget_);
}

void CustomDrag::OnDragBegin(GtkWidget* widget, GdkDragContext* drag_context) {
  if (image_)
    gtk_drag_set_icon_pixbuf(drag_context, image_->ToGdkPixbuf(), 0, 0);
}

void CustomDrag::OnDragEnd(GtkWidget* widget, GdkDragContext* drag_context) {
  delete this;
}
