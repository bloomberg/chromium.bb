// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GTK_CHROME_SHRINKABLE_HBOX_H_
#define CHROME_BROWSER_UI_GTK_GTK_CHROME_SHRINKABLE_HBOX_H_
#pragma once

#include <gdk/gdk.h>
#include <gtk/gtk.h>

// A specialized container derived from GtkHBox, which can shrink or hide its
// children one by one to fit into its width.
//
// Limitations of this container:
// - All children should have the same pack type, otherwise they may be
//   overlapped with each other.
// - All children must be packed with expand == false and fill == false,
//   otherwise they may be overlapped with each other.
// - The visibility of a child is adjusted automatically according to the
//   container's width. The child may not show or hide itself.

G_BEGIN_DECLS

#define GTK_TYPE_CHROME_SHRINKABLE_HBOX                                 \
    (gtk_chrome_shrinkable_hbox_get_type())
#define GTK_CHROME_SHRINKABLE_HBOX(obj)                                 \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_CHROME_SHRINKABLE_HBOX, \
                                GtkChromeShrinkableHBox))
#define GTK_CHROME_SHRINKABLE_HBOX_CLASS(klass)                         \
    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_CHROME_SHRINKABLE_HBOX,  \
                             GtkChromeShrinkableHBoxClass))
#define GTK_IS_CHROME_SHRINKABLE_HBOX(obj)                              \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_CHROME_SHRINKABLE_HBOX))
#define GTK_IS_CHROME_SHRINKABLE_HBOX_CLASS(klass)                      \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_CHROME_SHRINKABLE_HBOX))
#define GTK_CHROME_SHRINKABLE_HBOX_GET_CLASS(obj)                       \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_CHROME_SHRINKABLE_HBOX,  \
                               GtkChromeShrinkableHBoxClass))

typedef struct _GtkChromeShrinkableHBox GtkChromeShrinkableHBox;
typedef struct _GtkChromeShrinkableHBoxClass GtkChromeShrinkableHBoxClass;

struct _GtkChromeShrinkableHBox {
  // Parent class.
  GtkHBox hbox;

  gboolean hide_child_directly;

  // Private
  int children_width_requisition;
};

struct _GtkChromeShrinkableHBoxClass {
  GtkHBoxClass parent_class;
};

GType gtk_chrome_shrinkable_hbox_get_type() G_GNUC_CONST;

// Creates a new shrinkable hbox.
// If |hide_child_directly| is true then its child widgets will be hid directly
// if they are too wide to be fit into the hbox's width. Otherwise they will be
// shrunk first before being hid completely.
GtkWidget* gtk_chrome_shrinkable_hbox_new(gboolean hide_child_directly,
                                          gboolean homogeneous,
                                          gint spacing);

void gtk_chrome_shrinkable_hbox_set_hide_child_directly(
    GtkChromeShrinkableHBox* box, gboolean hide_child_directly);

gboolean gtk_chrome_shrinkable_hbox_get_hide_child_directly(
    GtkChromeShrinkableHBox* box);

void gtk_chrome_shrinkable_hbox_pack_start(GtkChromeShrinkableHBox* box,
                                           GtkWidget* child,
                                           guint padding);

void gtk_chrome_shrinkable_hbox_pack_end(GtkChromeShrinkableHBox* box,
                                         GtkWidget* child,
                                         guint padding);

gint gtk_chrome_shrinkable_hbox_get_visible_child_count(
    GtkChromeShrinkableHBox* box);

G_END_DECLS

#endif  // CHROME_BROWSER_UI_GTK_GTK_CHROME_SHRINKABLE_HBOX_H_
