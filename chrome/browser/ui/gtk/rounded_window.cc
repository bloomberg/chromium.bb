// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/rounded_window.h"

#include <gtk/gtk.h>
#include <math.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/base/gtk/gtk_signal_registrar.h"

namespace gtk_util {

namespace {

const char* kRoundedData = "rounded-window-data";

// If the border radius is less than |kMinRoundedBorderSize|, we don't actually
// round the corners, we just truncate the corner.
const int kMinRoundedBorderSize = 8;

struct RoundedWindowData {
  // Expected window size. Used to detect when we need to reshape the window.
  int expected_width;
  int expected_height;

  // Color of the border.
  GdkColor border_color;

  // Radius of the edges in pixels.
  int corner_size;

  // Which corners should be rounded?
  int rounded_edges;

  // Which sides of the window should have an internal border?
  int drawn_borders;

  // Keeps track of attached signal handlers.
  ui::GtkSignalRegistrar signals;
};

// Callback from GTK to release allocated memory.
void FreeRoundedWindowData(gpointer data) {
  delete static_cast<RoundedWindowData*>(data);
}

enum FrameType {
  FRAME_MASK,
  FRAME_STROKE,
};

// Returns a list of points that either form the outline of the status bubble
// (|type| == FRAME_MASK) or form the inner border around the inner edge
// (|type| == FRAME_STROKE).
std::vector<GdkPoint> MakeFramePolygonPoints(RoundedWindowData* data,
                                             FrameType type) {
  using gtk_util::MakeBidiGdkPoint;
  int width = data->expected_width;
  int height = data->expected_height;
  int corner_size = data->corner_size;

  std::vector<GdkPoint> points;

  bool ltr = !base::i18n::IsRTL();
  // If we have a stroke, we have to offset some of our points by 1 pixel.
  // We have to inset by 1 pixel when we draw horizontal lines that are on the
  // bottom or when we draw vertical lines that are closer to the end (end is
  // right for ltr).
  int y_off = (type == FRAME_MASK) ? 0 : -1;
  // We use this one for LTR.
  int x_off_l = ltr ? y_off : 0;
  // We use this one for RTL.
  int x_off_r = !ltr ? -y_off : 0;

  // Build up points starting with the bottom left corner and continuing
  // clockwise.

  // Bottom left corner.
  if (type == FRAME_MASK ||
      (data->drawn_borders & (BORDER_LEFT | BORDER_BOTTOM))) {
    if (data->rounded_edges & ROUNDED_BOTTOM_LEFT) {
      if (corner_size >= kMinRoundedBorderSize) {
        // We are careful to only add points that are horizontal or vertically
        // offset from the previous point (not both).  This avoids rounding
        // differences when two points are connected.
        for (int x = 0; x <= corner_size; ++x) {
          int y = static_cast<int>(sqrt(static_cast<double>(
              (corner_size * corner_size) - (x * x))));
          if (x > 0) {
            points.push_back(MakeBidiGdkPoint(
                corner_size - x + x_off_r + 1,
                height - (corner_size - y) + y_off, width, ltr));
          }
          points.push_back(MakeBidiGdkPoint(
              corner_size - x + x_off_r,
              height - (corner_size - y) + y_off, width, ltr));
        }
      } else {
        points.push_back(MakeBidiGdkPoint(
            corner_size + x_off_l, height + y_off, width, ltr));
        points.push_back(MakeBidiGdkPoint(
            x_off_r, height - corner_size, width, ltr));
      }
    } else {
      points.push_back(MakeBidiGdkPoint(x_off_r, height + y_off, width, ltr));
    }
  }

  // Top left corner.
  if (type == FRAME_MASK ||
      (data->drawn_borders & (BORDER_LEFT | BORDER_TOP))) {
    if (data->rounded_edges & ROUNDED_TOP_LEFT) {
      if (corner_size >= kMinRoundedBorderSize) {
        for (int x = corner_size; x >= 0; --x) {
          int y = static_cast<int>(sqrt(static_cast<double>(
              (corner_size * corner_size) - (x * x))));
          points.push_back(MakeBidiGdkPoint(corner_size - x + x_off_r,
              corner_size - y, width, ltr));
          if (x > 0) {
            points.push_back(MakeBidiGdkPoint(corner_size - x + 1 + x_off_r,
                corner_size - y, width, ltr));
          }
        }
      } else {
        points.push_back(MakeBidiGdkPoint(
            x_off_r, corner_size - 1, width, ltr));
        points.push_back(MakeBidiGdkPoint(
            corner_size + x_off_r - 1, 0, width, ltr));
      }
    } else {
      points.push_back(MakeBidiGdkPoint(x_off_r, 0, width, ltr));
    }
  }

  // Top right corner.
  if (type == FRAME_MASK ||
      (data->drawn_borders & (BORDER_TOP | BORDER_RIGHT))) {
    if (data->rounded_edges & ROUNDED_TOP_RIGHT) {
      if (corner_size >= kMinRoundedBorderSize) {
        for (int x = 0; x <= corner_size; ++x) {
          int y = static_cast<int>(sqrt(static_cast<double>(
              (corner_size * corner_size) - (x * x))));
          if (x > 0) {
            points.push_back(MakeBidiGdkPoint(
                width - (corner_size - x) + x_off_l - 1,
                corner_size - y, width, ltr));
          }
          points.push_back(MakeBidiGdkPoint(
              width - (corner_size - x) + x_off_l,
              corner_size - y, width, ltr));
        }
      } else {
        points.push_back(MakeBidiGdkPoint(
            width - corner_size + 1 + x_off_l, 0, width, ltr));
        points.push_back(MakeBidiGdkPoint(
            width + x_off_l, corner_size - 1, width, ltr));
      }
    } else {
      points.push_back(MakeBidiGdkPoint(
          width + x_off_l, 0, width, ltr));
    }
  }

  // Bottom right corner.
  if (type == FRAME_MASK ||
      (data->drawn_borders & (BORDER_RIGHT | BORDER_BOTTOM))) {
    if (data->rounded_edges & ROUNDED_BOTTOM_RIGHT) {
      if (corner_size >= kMinRoundedBorderSize) {
        for (int x = corner_size; x >= 0; --x) {
          int y = static_cast<int>(sqrt(static_cast<double>(
              (corner_size * corner_size) - (x * x))));
          points.push_back(MakeBidiGdkPoint(
              width - (corner_size - x) + x_off_l,
              height - (corner_size - y) + y_off, width, ltr));
          if (x > 0) {
            points.push_back(MakeBidiGdkPoint(
                width - (corner_size - x) + x_off_l - 1,
                height - (corner_size - y) + y_off, width, ltr));
          }
        }
      } else {
        points.push_back(MakeBidiGdkPoint(
            width + x_off_l, height - corner_size, width, ltr));
        points.push_back(MakeBidiGdkPoint(
            width - corner_size + x_off_r, height + y_off, width, ltr));
      }
    } else {
      points.push_back(MakeBidiGdkPoint(
          width + x_off_l, height + y_off, width, ltr));
    }
  }

  return points;
}

// Set the window shape in needed, lets our owner do some drawing (if it wants
// to), and finally draw the border.
gboolean OnRoundedWindowExpose(GtkWidget* widget,
                               GdkEventExpose* event) {
  RoundedWindowData* data = static_cast<RoundedWindowData*>(
      g_object_get_data(G_OBJECT(widget), kRoundedData));

  if (data->expected_width != widget->allocation.width ||
      data->expected_height != widget->allocation.height) {
    data->expected_width = widget->allocation.width;
    data->expected_height = widget->allocation.height;

    // We need to update the shape of the status bubble whenever our GDK
    // window changes shape.
    std::vector<GdkPoint> mask_points = MakeFramePolygonPoints(
        data, FRAME_MASK);
    GdkRegion* mask_region = gdk_region_polygon(&mask_points[0],
                                                mask_points.size(),
                                                GDK_EVEN_ODD_RULE);
    gdk_window_shape_combine_region(widget->window, mask_region, 0, 0);
    gdk_region_destroy(mask_region);
  }

  GdkDrawable* drawable = GDK_DRAWABLE(event->window);
  GdkGC* gc = gdk_gc_new(drawable);
  gdk_gc_set_clip_rectangle(gc, &event->area);
  gdk_gc_set_rgb_fg_color(gc, &data->border_color);

  // Stroke the frame border.
  std::vector<GdkPoint> points = MakeFramePolygonPoints(
      data, FRAME_STROKE);
  if (data->drawn_borders == BORDER_ALL) {
    // If we want to have borders everywhere, we need to draw a polygon instead
    // of a set of lines.
    gdk_draw_polygon(drawable, gc, FALSE, &points[0], points.size());
  } else if (!points.empty()) {
    gdk_draw_lines(drawable, gc, &points[0], points.size());
  }

  g_object_unref(gc);
  return FALSE;  // Propagate so our children paint, etc.
}

// On theme changes, window shapes are reset, but we detect whether we need to
// reshape a window by whether its allocation has changed so force it to reset
// the window shape on next expose.
void OnStyleSet(GtkWidget* widget, GtkStyle* previous_style) {
  DCHECK(widget);
  RoundedWindowData* data = static_cast<RoundedWindowData*>(
      g_object_get_data(G_OBJECT(widget), kRoundedData));
  DCHECK(data);
  data->expected_width = -1;
  data->expected_height = -1;
}

}  // namespace

void ActAsRoundedWindow(
    GtkWidget* widget, const GdkColor& color, int corner_size,
    int rounded_edges, int drawn_borders) {
  DCHECK(widget);
  DCHECK(!g_object_get_data(G_OBJECT(widget), kRoundedData));

  gtk_widget_set_app_paintable(widget, TRUE);

  RoundedWindowData* data = new RoundedWindowData;
  data->signals.Connect(widget, "expose-event",
                        G_CALLBACK(OnRoundedWindowExpose), NULL);
  data->signals.Connect(widget, "style-set", G_CALLBACK(OnStyleSet), NULL);

  data->expected_width = -1;
  data->expected_height = -1;

  data->border_color = color;
  data->corner_size = corner_size;

  data->rounded_edges = rounded_edges;
  data->drawn_borders = drawn_borders;

  g_object_set_data_full(G_OBJECT(widget), kRoundedData,
                         data, FreeRoundedWindowData);

  if (GTK_WIDGET_VISIBLE(widget))
    gtk_widget_queue_draw(widget);
}

void StopActingAsRoundedWindow(GtkWidget* widget) {
  g_object_set_data(G_OBJECT(widget), kRoundedData, NULL);

  if (GTK_WIDGET_REALIZED(widget))
    gdk_window_shape_combine_mask(widget->window, NULL, 0, 0);

  if (GTK_WIDGET_VISIBLE(widget))
    gtk_widget_queue_draw(widget);
}

bool IsActingAsRoundedWindow(GtkWidget* widget) {
  return g_object_get_data(G_OBJECT(widget), kRoundedData) != NULL;
}

void SetRoundedWindowEdgesAndBorders(GtkWidget* widget,
                                     int corner_size,
                                     int rounded_edges,
                                     int drawn_borders) {
  DCHECK(widget);
  RoundedWindowData* data = static_cast<RoundedWindowData*>(
      g_object_get_data(G_OBJECT(widget), kRoundedData));
  DCHECK(data);
  data->corner_size = corner_size;
  data->rounded_edges = rounded_edges;
  data->drawn_borders = drawn_borders;
}

void SetRoundedWindowBorderColor(GtkWidget* widget, GdkColor color) {
  DCHECK(widget);
  RoundedWindowData* data = static_cast<RoundedWindowData*>(
      g_object_get_data(G_OBJECT(widget), kRoundedData));
  DCHECK(data);
  data->border_color = color;
}

}  // namespace gtk_util
