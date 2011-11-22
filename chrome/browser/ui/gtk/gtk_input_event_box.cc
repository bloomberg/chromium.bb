// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_input_event_box.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_INPUT_EVENT_BOX_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                            GTK_TYPE_INPUT_EVENT_BOX, \
                                            GtkInputEventBoxPrivate))

typedef struct _GtkInputEventBoxPrivate GtkInputEventBoxPrivate;

struct _GtkInputEventBoxPrivate {
  GdkWindow* event_window;
};

G_DEFINE_TYPE(GtkInputEventBox, gtk_input_event_box, GTK_TYPE_BIN)

static void gtk_input_event_box_realize(GtkWidget* widget);
static void gtk_input_event_box_unrealize(GtkWidget* widget);
static void gtk_input_event_box_map(GtkWidget* widget);
static void gtk_input_event_box_unmap(GtkWidget* widget);
static void gtk_input_event_box_size_allocate(GtkWidget* widget,
                                              GtkAllocation* allocation);
static void gtk_input_event_box_size_request(GtkWidget* widget,
                                             GtkRequisition* requisition);

static void gtk_input_event_box_class_init(GtkInputEventBoxClass* klass) {
  GtkWidgetClass* widget_class = reinterpret_cast<GtkWidgetClass*>(klass);
  widget_class->realize = gtk_input_event_box_realize;
  widget_class->unrealize = gtk_input_event_box_unrealize;
  widget_class->map = gtk_input_event_box_map;
  widget_class->unmap = gtk_input_event_box_unmap;
  widget_class->size_allocate = gtk_input_event_box_size_allocate;
  widget_class->size_request = gtk_input_event_box_size_request;

  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  g_type_class_add_private(gobject_class, sizeof(GtkInputEventBoxPrivate));
}

static void gtk_input_event_box_init(GtkInputEventBox* widget) {
  GtkInputEventBoxPrivate* priv = GTK_INPUT_EVENT_BOX_GET_PRIVATE(widget);
  priv->event_window = NULL;
}

GtkWidget* gtk_input_event_box_new() {
  return GTK_WIDGET(g_object_new(GTK_TYPE_INPUT_EVENT_BOX, NULL));
}

static void gtk_input_event_box_realize(GtkWidget* widget) {
  g_return_if_fail(GTK_IS_INPUT_EVENT_BOX(widget));

  GtkInputEventBoxPrivate* priv = GTK_INPUT_EVENT_BOX_GET_PRIVATE(widget);

  GdkWindow* parent = gtk_widget_get_parent_window(widget);
  GdkWindowAttr attributes;
  GtkAllocation allocation;
  gint attributes_mask = GDK_WA_X | GDK_WA_Y;

  gtk_widget_get_allocation(widget, &allocation);

  attributes.x = allocation.x;
  attributes.y = allocation.y;
  attributes.width = allocation.width;
  attributes.height = allocation.height;

  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.wclass = GDK_INPUT_ONLY;

  attributes.event_mask = gtk_widget_get_events(widget) |
      GDK_BUTTON_MOTION_MASK |
      GDK_BUTTON_PRESS_MASK |
      GDK_BUTTON_RELEASE_MASK |
      GDK_EXPOSURE_MASK |
      GDK_ENTER_NOTIFY_MASK |
      GDK_LEAVE_NOTIFY_MASK;

  priv->event_window = gdk_window_new(parent, &attributes, attributes_mask);
  gdk_window_set_user_data(priv->event_window, widget);

  GTK_WIDGET_CLASS(gtk_input_event_box_parent_class)->realize(widget);
}

static void gtk_input_event_box_unrealize(GtkWidget* widget) {
  g_return_if_fail(GTK_IS_INPUT_EVENT_BOX(widget));

  GtkInputEventBoxPrivate* priv = GTK_INPUT_EVENT_BOX_GET_PRIVATE(widget);
  if (priv->event_window) {
    gdk_window_set_user_data(priv->event_window, NULL);
    gdk_window_destroy(priv->event_window);
    priv->event_window = NULL;
  }

  GTK_WIDGET_CLASS(gtk_input_event_box_parent_class)->unrealize(widget);
}

static void gtk_input_event_box_map(GtkWidget* widget) {
  g_return_if_fail(GTK_IS_INPUT_EVENT_BOX(widget));

  GtkInputEventBoxPrivate* priv = GTK_INPUT_EVENT_BOX_GET_PRIVATE(widget);
  if (priv->event_window)
    gdk_window_show(priv->event_window);

  GTK_WIDGET_CLASS(gtk_input_event_box_parent_class)->map(widget);
}

static void gtk_input_event_box_unmap(GtkWidget* widget) {
  g_return_if_fail(GTK_IS_INPUT_EVENT_BOX(widget));

  GtkInputEventBoxPrivate* priv = GTK_INPUT_EVENT_BOX_GET_PRIVATE(widget);
  if (priv->event_window)
    gdk_window_hide(priv->event_window);

  GTK_WIDGET_CLASS(gtk_input_event_box_parent_class)->unmap(widget);
}

static void gtk_input_event_box_size_allocate(GtkWidget* widget,
                                              GtkAllocation* allocation) {
  g_return_if_fail(GTK_IS_INPUT_EVENT_BOX(widget));

  widget->allocation = *allocation;

  GtkInputEventBoxPrivate* priv = GTK_INPUT_EVENT_BOX_GET_PRIVATE(widget);
  if (priv->event_window) {
    gdk_window_move_resize(priv->event_window,
                           allocation->x,
                           allocation->y,
                           allocation->width,
                           allocation->height);
  }

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_widget_size_allocate(child, allocation);
}

static void gtk_input_event_box_size_request(GtkWidget* widget,
                                             GtkRequisition* requisition) {

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_widget_size_request(child, requisition);
}

GdkWindow* gtk_input_event_box_get_window(GtkInputEventBox* widget) {
  g_return_val_if_fail(GTK_IS_INPUT_EVENT_BOX(widget), NULL);

  GtkInputEventBoxPrivate* priv = GTK_INPUT_EVENT_BOX_GET_PRIVATE(widget);
  return priv->event_window;
}

G_END_DECLS
