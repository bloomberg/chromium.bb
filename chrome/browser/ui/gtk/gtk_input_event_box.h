// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_INPUT_EVENT_BOX_H_
#define CHROME_BROWSER_UI_GTK_GTK_INPUT_EVENT_BOX_H_
#pragma once

#include <gtk/gtk.h>

// GtkInputEventBox is like GtkEventBox, but with the following differences:
//  1. Only supports input (like gtk_event_box_set_visible_window(foo, FALSE).
//  2. Provides a method to get the GdkWindow (gtk_input_event_box_get_window).
//  (The GdkWindow created by GtkEventBox cannot be retrieved unless you use it
//  in visible mode.)
// TODO(mattm): Get rid of this class someday if Gtk adds a way to get the
// GtkEventBox event_window (https://bugzilla.gnome.org/show_bug.cgi?id=657380).

G_BEGIN_DECLS

#define GTK_TYPE_INPUT_EVENT_BOX                                 \
    (gtk_input_event_box_get_type())
#define GTK_INPUT_EVENT_BOX(obj)                                 \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_INPUT_EVENT_BOX, \
                                GtkInputEventBox))
#define GTK_INPUT_EVENT_BOX_CLASS(klass)                         \
    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_INPUT_EVENT_BOX,  \
                             GtkInputEventBoxClass))
#define GTK_IS_INPUT_EVENT_BOX(obj)                              \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_INPUT_EVENT_BOX))
#define GTK_IS_INPUT_EVENT_BOX_CLASS(klass)                      \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_INPUT_EVENT_BOX))
#define GTK_INPUT_EVENT_BOX_GET_CLASS(obj)                       \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_INPUT_EVENT_BOX,  \
                               GtkInputEventBoxClass))

typedef struct _GtkInputEventBox GtkInputEventBox;
typedef struct _GtkInputEventBoxClass GtkInputEventBoxClass;

struct _GtkInputEventBox {
  // Parent class.
  GtkBin bin;
};

struct _GtkInputEventBoxClass {
  GtkBinClass parent_class;
};

GType gtk_input_event_box_get_type();
GtkWidget* gtk_input_event_box_new();

// Get the GdkWindow |widget| uses for handling input events. Will be NULL if
// the widget has not been realized yet.
GdkWindow* gtk_input_event_box_get_window(GtkInputEventBox* widget);

G_END_DECLS

#endif  // CHROME_BROWSER_UI_GTK_GTK_INPUT_EVENT_BOX_H_
