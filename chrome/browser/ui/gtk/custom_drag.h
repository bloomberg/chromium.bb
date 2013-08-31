// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CUSTOM_DRAG_H_
#define CHROME_BROWSER_UI_GTK_CUSTOM_DRAG_H_

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "ui/base/gtk/gtk_signal.h"

namespace gfx {
class Image;
}

// Base class for programatically generated drags.
class CustomDrag {
 protected:
  CustomDrag(gfx::Image* icon, int code_mask, GdkDragAction action);
  virtual ~CustomDrag();

  virtual void OnDragDataGet(GtkWidget* widget,
                             GdkDragContext* context,
                             GtkSelectionData* selection_data,
                             guint target_type,
                             guint time) = 0;

 private:
  CHROMEGTK_CALLBACK_1(CustomDrag, void, OnDragBegin, GdkDragContext*);
  CHROMEGTK_CALLBACK_1(CustomDrag, void, OnDragEnd, GdkDragContext*);

  // Since this uses a virtual function, we can't use a macro.
  static void OnDragDataGetThunk(GtkWidget* widget, GdkDragContext* context,
                                 GtkSelectionData* selection_data,
                                 guint target_type, guint time,
                                 CustomDrag* custom_drag) {
    return custom_drag->OnDragDataGet(widget, context, selection_data,
                                      target_type, time);
  }

  // Can't use a OwnedWidgetGtk because the initialization of GtkInvisible
  // sinks the reference.
  GtkWidget* drag_widget_;

  // The image for the drag. The lifetime of the image should be managed outside
  // this object. Most icons are owned by the IconManager.
  gfx::Image* image_;

  DISALLOW_COPY_AND_ASSIGN(CustomDrag);
};

#endif  // CHROME_BROWSER_UI_GTK_CUSTOM_DRAG_H_
