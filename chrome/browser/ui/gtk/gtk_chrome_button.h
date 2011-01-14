// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_CHROME_BUTTON_H_
#define CHROME_BROWSER_UI_GTK_GTK_CHROME_BUTTON_H_
#pragma once

#include <gdk/gdk.h>
#include <gtk/gtkbutton.h>

G_BEGIN_DECLS

#define GTK_TYPE_CHROME_BUTTON        (gtk_chrome_button_get_type ())
#define GTK_CHROME_BUTTON(obj)                              \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_CHROME_BUTTON, GtkChromeButton))
#define GTK_CHROME_BUTTON_CLASS(klass)                      \
  (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_CHROME_BUTTON, \
                           GtkChromeButtonClass))
#define GTK_IS_CHROME_BUTTON(obj)                           \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_CHROME_BUTTON))
#define GTK_IS_CHROME_BUTTON_CLASS(klass)                   \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_CHROME_BUTTON))
#define GTK_CHROME_BUTTON_GET_CLASS(obj)                    \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_CHROME_BUTTON, GtkChromeButton))

typedef struct _GtkChromeButton        GtkChromeButton;
typedef struct _GtkChromeButtonClass   GtkChromeButtonClass;

struct _GtkChromeButton {
  GtkButton button;
};

struct _GtkChromeButtonClass {
  GtkButtonClass parent_class;
};

GtkWidget* gtk_chrome_button_new();

GType gtk_chrome_button_get_type();

// Set the paint state to |state|. This overrides the widget's current state.
void gtk_chrome_button_set_paint_state(GtkChromeButton* button,
                                       GtkStateType state);

// Revert to using the widget's current state for painting.
void gtk_chrome_button_unset_paint_state(GtkChromeButton* button);

// Whether we should use custom theme images or let GTK take care of it.
void gtk_chrome_button_set_use_gtk_rendering(GtkChromeButton* button,
                                             gboolean value);

// Sets the partial hover state of the button. The acceptable range is 0.0 to
// 1.0. If |state| is outside of that range, then revert the button to normal
// hovering. This can be overridden by gtk_chrome_button_set_paint_state.
void gtk_chrome_button_set_hover_state(GtkChromeButton* button,
                                       gdouble state);

G_END_DECLS

#endif  // CHROME_BROWSER_UI_GTK_GTK_CHROME_BUTTON_H_
