// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_chrome_shrinkable_hbox.h"

#include <gtk/gtk.h>

#include <algorithm>

namespace {

enum {
  PROP_0,
  PROP_HIDE_CHILD_DIRECTLY
};

struct SizeAllocateData {
  GtkChromeShrinkableHBox* box;
  GtkAllocation* allocation;
  GtkTextDirection direction;
  bool homogeneous;
  int border_width;

  // Maximum child width when |homogeneous| is TRUE.
  int homogeneous_child_width;
};

void CountVisibleChildren(GtkWidget* child, gpointer userdata) {
  if (GTK_WIDGET_VISIBLE(child))
    ++(*reinterpret_cast<int*>(userdata));
}

void SumChildrenWidthRequisition(GtkWidget* child, gpointer userdata) {
  if (GTK_WIDGET_VISIBLE(child)) {
    GtkRequisition req;
    gtk_widget_get_child_requisition(child, &req);
    (*reinterpret_cast<int*>(userdata)) += std::max(req.width, 0);
  }
}

void ShowInvisibleChildren(GtkWidget* child, gpointer userdata) {
  if (!GTK_WIDGET_VISIBLE(child)) {
    gtk_widget_show(child);
    ++(*reinterpret_cast<int*>(userdata));
  }
}

void ChildSizeAllocate(GtkWidget* child, gpointer userdata) {
  if (!GTK_WIDGET_VISIBLE(child))
    return;

  SizeAllocateData* data = reinterpret_cast<SizeAllocateData*>(userdata);
  GtkAllocation child_allocation = child->allocation;

  if (data->homogeneous) {
    // Make sure the child is not overlapped with others' boundary.
    if (child_allocation.width > data->homogeneous_child_width) {
      child_allocation.x +=
          (child_allocation.width - data->homogeneous_child_width) / 2;
      child_allocation.width = data->homogeneous_child_width;
    }
  } else {
    guint padding;
    GtkPackType pack_type;
    gtk_box_query_child_packing(GTK_BOX(data->box), child, NULL, NULL,
                                &padding, &pack_type);

    if ((data->direction == GTK_TEXT_DIR_RTL && pack_type == GTK_PACK_START) ||
        (data->direction != GTK_TEXT_DIR_RTL && pack_type == GTK_PACK_END)) {
      // All children are right aligned, so make sure the child won't overflow
      // its parent's left edge.
      int overflow = (data->allocation->x + data->border_width + padding -
                      child_allocation.x);
      if (overflow > 0) {
        child_allocation.width -= overflow;
        child_allocation.x += overflow;
      }
    } else {
      // All children are left aligned, so make sure the child won't overflow
      // its parent's right edge.
      int overflow = (child_allocation.x + child_allocation.width + padding -
          (data->allocation->x + data->allocation->width - data->border_width));
      if (overflow > 0)
        child_allocation.width -= overflow;
    }
  }

  if (child_allocation.width != child->allocation.width) {
    if (data->box->hide_child_directly || child_allocation.width <= 1)
      gtk_widget_hide(child);
    else
      gtk_widget_size_allocate(child, &child_allocation);
  }
}

}  // namespace

G_BEGIN_DECLS

static void gtk_chrome_shrinkable_hbox_set_property(GObject* object,
                                             guint prop_id,
                                             const GValue* value,
                                             GParamSpec* pspec);
static void gtk_chrome_shrinkable_hbox_get_property(GObject* object,
                                             guint prop_id,
                                             GValue* value,
                                             GParamSpec* pspec);
static void gtk_chrome_shrinkable_hbox_size_allocate(GtkWidget* widget,
                                              GtkAllocation* allocation);

G_DEFINE_TYPE(GtkChromeShrinkableHBox, gtk_chrome_shrinkable_hbox,
              GTK_TYPE_HBOX)

static void gtk_chrome_shrinkable_hbox_class_init(
    GtkChromeShrinkableHBoxClass *klass) {
  GObjectClass* object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property = gtk_chrome_shrinkable_hbox_set_property;
  object_class->get_property = gtk_chrome_shrinkable_hbox_get_property;

  widget_class->size_allocate = gtk_chrome_shrinkable_hbox_size_allocate;

  g_object_class_install_property(object_class, PROP_HIDE_CHILD_DIRECTLY,
      g_param_spec_boolean("hide-child-directly",
                           "Hide child directly",
                           "Whether the children should be hid directly, "
                           "if there is no enough space in its parent",
                           FALSE,
                           static_cast<GParamFlags>(
                               G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

static void gtk_chrome_shrinkable_hbox_init(GtkChromeShrinkableHBox* box) {
  box->hide_child_directly = FALSE;
  box->children_width_requisition = 0;
}

static void gtk_chrome_shrinkable_hbox_set_property(GObject* object,
                                                    guint prop_id,
                                                    const GValue* value,
                                                    GParamSpec* pspec) {
  GtkChromeShrinkableHBox* box = GTK_CHROME_SHRINKABLE_HBOX(object);

  switch (prop_id) {
    case PROP_HIDE_CHILD_DIRECTLY:
      gtk_chrome_shrinkable_hbox_set_hide_child_directly(
          box, g_value_get_boolean(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gtk_chrome_shrinkable_hbox_get_property(GObject* object,
                                                    guint prop_id,
                                                    GValue* value,
                                                    GParamSpec* pspec) {
  GtkChromeShrinkableHBox* box = GTK_CHROME_SHRINKABLE_HBOX(object);

  switch (prop_id) {
    case PROP_HIDE_CHILD_DIRECTLY:
      g_value_set_boolean(value, box->hide_child_directly);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
      break;
  }
}

static void gtk_chrome_shrinkable_hbox_size_allocate(
    GtkWidget* widget, GtkAllocation* allocation) {
  GtkChromeShrinkableHBox* box = GTK_CHROME_SHRINKABLE_HBOX(widget);
  gint children_width_requisition = 0;
  gtk_container_foreach(GTK_CONTAINER(widget), SumChildrenWidthRequisition,
                        &children_width_requisition);

  // If we are allocated to more width or some children are removed or shrunk,
  // then we need to show all invisible children before calling parent class's
  // size_allocate method, because the new width may be enough to show those
  // hidden children.
  if (widget->allocation.width < allocation->width ||
      box->children_width_requisition > children_width_requisition) {
    gtk_container_foreach(GTK_CONTAINER(widget),
                          reinterpret_cast<GtkCallback>(gtk_widget_show), NULL);

    // If there were any invisible children, showing them will trigger another
    // allocate. But we still need to go through the size allocate process
    // in this iteration, otherwise before the next allocate iteration, the
    // children may be redrawn on the screen with incorrect size allocation.
  }

  // Let the parent class do size allocation first. After that all children will
  // be allocated with reasonable position and size according to their size
  // request.
  (GTK_WIDGET_CLASS(gtk_chrome_shrinkable_hbox_parent_class)->size_allocate)
      (widget, allocation);

  gint visible_children_count =
      gtk_chrome_shrinkable_hbox_get_visible_child_count(
          GTK_CHROME_SHRINKABLE_HBOX(widget));

  box->children_width_requisition = 0;
  if (visible_children_count == 0)
    return;

  SizeAllocateData data;
  data.box = GTK_CHROME_SHRINKABLE_HBOX(widget);
  data.allocation = allocation;
  data.direction = gtk_widget_get_direction(widget);
  data.homogeneous = gtk_box_get_homogeneous(GTK_BOX(widget));
  data.border_width = gtk_container_get_border_width(GTK_CONTAINER(widget));
  data.homogeneous_child_width =
      (allocation->width - data.border_width * 2 -
       (visible_children_count - 1) * gtk_box_get_spacing(GTK_BOX(widget))) /
      visible_children_count;

  // Shrink or hide children if necessary.
  gtk_container_foreach(GTK_CONTAINER(widget), ChildSizeAllocate, &data);

  // Record current width requisition of visible children, so we can know if
  // it's necessary to show invisible children next time.
  gtk_container_foreach(GTK_CONTAINER(widget), SumChildrenWidthRequisition,
                        &box->children_width_requisition);
}

GtkWidget* gtk_chrome_shrinkable_hbox_new(gboolean hide_child_directly,
                                          gboolean homogeneous,
                                          gint spacing) {
  return GTK_WIDGET(g_object_new(GTK_TYPE_CHROME_SHRINKABLE_HBOX,
                                 "hide-child-directly", hide_child_directly,
                                 "homogeneous", homogeneous,
                                 "spacing", spacing,
                                 NULL));
}

void gtk_chrome_shrinkable_hbox_set_hide_child_directly(
    GtkChromeShrinkableHBox* box, gboolean hide_child_directly) {
  g_return_if_fail(GTK_IS_CHROME_SHRINKABLE_HBOX(box));

  if (hide_child_directly != box->hide_child_directly) {
    box->hide_child_directly = hide_child_directly;
    g_object_notify(G_OBJECT(box), "hide-child-directly");
    gtk_widget_queue_resize(GTK_WIDGET(box));
  }
}

gboolean gtk_chrome_shrinkable_hbox_get_hide_child_directly(
    GtkChromeShrinkableHBox* box) {
  g_return_val_if_fail(GTK_IS_CHROME_SHRINKABLE_HBOX(box), FALSE);

  return box->hide_child_directly;
}

void gtk_chrome_shrinkable_hbox_pack_start(GtkChromeShrinkableHBox* box,
                                           GtkWidget* child,
                                           guint padding) {
  g_return_if_fail(GTK_IS_CHROME_SHRINKABLE_HBOX(box));
  g_return_if_fail(GTK_IS_WIDGET(child));

  gtk_box_pack_start(GTK_BOX(box), child, FALSE, FALSE, 0);
}

void gtk_chrome_shrinkable_hbox_pack_end(GtkChromeShrinkableHBox* box,
                                         GtkWidget* child,
                                         guint padding) {
  g_return_if_fail(GTK_IS_CHROME_SHRINKABLE_HBOX(box));
  g_return_if_fail(GTK_IS_WIDGET(child));

  gtk_box_pack_end(GTK_BOX(box), child, FALSE, FALSE, 0);
}

gint gtk_chrome_shrinkable_hbox_get_visible_child_count(
    GtkChromeShrinkableHBox* box) {
  gint visible_children_count = 0;
  gtk_container_foreach(GTK_CONTAINER(box), CountVisibleChildren,
                        &visible_children_count);
  return visible_children_count;
}

G_END_DECLS
