// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Creates a link button that shows |text| in blue and underlined. The cursor
// changes to a hand when over the link.  This is like the GTK LinkButton, but
// it doesn't call the global URI link handler, etc.  It is a button subclass,
// so you can just handle the clicked signal.

#ifndef CHROME_BROWSER_UI_GTK_GTK_CHROME_LINK_BUTTON_H_
#define CHROME_BROWSER_UI_GTK_GTK_CHROME_LINK_BUTTON_H_
#pragma once

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_CHROME_LINK_BUTTON        (gtk_chrome_link_button_get_type ())
#define GTK_CHROME_LINK_BUTTON(obj)        (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                                            GTK_TYPE_CHROME_LINK_BUTTON, \
                                            GtkChromeLinkButton))
#define GTK_CHROME_LINK_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), \
                                             GTK_TYPE_CHROME_LINK_BUTTON, \
                                             GtkChromeLinkButtonClass))
#define GTK_IS_CHROME_LINK_BUTTON(obj)                           \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_CHROME_LINK_BUTTON))
#define GTK_IS_CHROME_LINK_BUTTON_CLASS(klass)                   \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_CHROME_LINK_BUTTON))
#define GTK_CHROME_LINK_BUTTON_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_CHROME_LINK_BUTTON, \
                             GtkChromeLinkButton))

typedef struct _GtkChromeLinkButton        GtkChromeLinkButton;
typedef struct _GtkChromeLinkButtonClass   GtkChromeLinkButtonClass;

struct _GtkChromeLinkButton {
  GtkButton button;
  GtkWidget* label;
  gchar* normal_markup;
  gchar* pressed_markup;
  gboolean is_normal;
  gchar normal_color[9];
  gchar* native_markup;
  gboolean using_native_theme;
  GdkCursor* hand_cursor;
  gchar* text;
  gboolean uses_markup;
};

struct _GtkChromeLinkButtonClass {
  GtkButtonClass parent_class;
};

// Make a link button with display text |text|.
GtkWidget* gtk_chrome_link_button_new(const char* text);

// As above, but don't escape markup in the text.
GtkWidget* gtk_chrome_link_button_new_with_markup(const char* markup);

// Set whether the link button draws natively (using "link-color"). The default
// is TRUE.
void gtk_chrome_link_button_set_use_gtk_theme(GtkChromeLinkButton* button,
                                              gboolean use_gtk);

// Set the label text of the link.
void gtk_chrome_link_button_set_label(GtkChromeLinkButton* button,
                                      const char* text);

// Set the color when the link is in a normal state (i.e. not pressed).
// If not set, or called NULL |color|, the color will be blue.
void gtk_chrome_link_button_set_normal_color(GtkChromeLinkButton* button,
                                             const GdkColor* color);

GType gtk_chrome_link_button_get_type();

G_END_DECLS

#endif  // CHROME_BROWSER_UI_GTK_GTK_CHROME_LINK_BUTTON_H_
