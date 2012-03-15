// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_overflow_indicator_gtk.h"

#include <string>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "ui/base/gtk/gtk_hig_constants.h"

namespace {

// The height in pixels of the widget to show the plus count.
const int kWidgetHeight = 15;

// Rounded corner size.
const int kRoundedCornerSize = 4;

// Metrics for the indicator text.
const int kTextFontSize = 10 * PANGO_SCALE;
const GdkColor kTextColor = ui::kGdkWhite;

// Gradient colors used to draw the background in normal mode.
const GdkColor kNormalBackgroundColorStart =
    GDK_COLOR_RGB(0x77, 0x77, 0x77);
const GdkColor kNormalBackgroundColorEnd =
    GDK_COLOR_RGB(0x55, 0x55, 0x55);

// Gradient colors used to draw the background in attention mode.
const GdkColor kAttentionBackgroundColorStart =
    GDK_COLOR_RGB(0xFF, 0xAB, 0x57);
const GdkColor kAttentionBackgroundColorEnd =
    GDK_COLOR_RGB(0xF5, 0x93, 0x38);

}  // namespace

PanelOverflowIndicator* PanelOverflowIndicator::Create() {
  return new PanelOverflowIndicatorGtk();
}

PanelOverflowIndicatorGtk::PanelOverflowIndicatorGtk()
    : count_(0),
      is_drawing_attention_(false) {
  window_ = gtk_window_new(GTK_WINDOW_POPUP);

  title_ = gtk_label_new(NULL);
  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  pango_attr_list_insert(attributes,
                         pango_attr_foreground_new(kTextColor.red,
                                                   kTextColor.green,
                                                   kTextColor.blue));
  pango_attr_list_insert(attributes,
                         pango_attr_size_new_absolute(kTextFontSize));
  gtk_label_set_attributes(GTK_LABEL(title_), attributes);
  pango_attr_list_unref(attributes);
  gtk_container_add(GTK_CONTAINER(window_), title_);
  gtk_widget_show(title_);

  gtk_util::ActAsRoundedWindow(window_, GdkColor(), kRoundedCornerSize,
                               gtk_util::ROUNDED_TOP_RIGHT,
                               gtk_util::BORDER_NONE);

  g_signal_connect(window_, "expose-event",
                   G_CALLBACK(OnExposeThunk), this);
}

PanelOverflowIndicatorGtk::~PanelOverflowIndicatorGtk() {
  gtk_widget_destroy(window_);
}

int PanelOverflowIndicatorGtk::GetHeight() const {
  return kWidgetHeight;
}

gfx::Rect PanelOverflowIndicatorGtk::GetBounds() const {
  return bounds_;
}

void PanelOverflowIndicatorGtk::SetBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;
  bounds_ = bounds;
  gtk_window_move(GTK_WINDOW(window_), bounds.x(), bounds.y());
  gtk_window_resize(GTK_WINDOW(window_), bounds.width(), bounds.height());
}

int PanelOverflowIndicatorGtk::GetCount() const {
  return count_;
}

void PanelOverflowIndicatorGtk::SetCount(int count) {
  if (count_ == count)
    return;
  count_ = count;

  if (count_ > 0) {
    std::string title_text = "+" + base::IntToString(count_);
    gtk_label_set_text(GTK_LABEL(title_), title_text.c_str());

    gtk_widget_show(window_);
  } else {
    gtk_widget_hide(window_);
  }
}

void PanelOverflowIndicatorGtk::DrawAttention() {
  if (is_drawing_attention_)
    return;
  is_drawing_attention_ = true;
  gtk_widget_queue_draw(window_);
}

void PanelOverflowIndicatorGtk::StopDrawingAttention() {
  if (!is_drawing_attention_)
    return;
  is_drawing_attention_ = false;
  gtk_widget_queue_draw(window_);
}

bool PanelOverflowIndicatorGtk::IsDrawingAttention() const {
  return is_drawing_attention_;
}

gboolean PanelOverflowIndicatorGtk::OnExpose(GtkWidget* widget,
                                             GdkEventExpose* event) {
  cairo_t* cr = gdk_cairo_create(gtk_widget_get_window(widget));
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  // Draw background color.
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  cairo_pattern_t* pattern = cairo_pattern_create_linear(
      allocation.x, allocation.y,
      allocation.x + allocation.width, allocation.y);

  const GdkColor* color_start;
  const GdkColor* color_end;
  if (is_drawing_attention_) {
    color_start = &kAttentionBackgroundColorStart;
    color_end = &kAttentionBackgroundColorEnd;
  } else {
    color_start = &kNormalBackgroundColorStart;
    color_end = &kNormalBackgroundColorEnd;
  }

  cairo_pattern_add_color_stop_rgb(pattern, 0.0,
                                   color_start->red / 65535.0,
                                   color_start->green / 65535.0,
                                   color_start->blue / 65535.0);
  cairo_pattern_add_color_stop_rgb(pattern, 1.0,
                                   color_end->red / 65535.0,
                                   color_end->green / 65535.0,
                                   color_end->blue / 65535.0);

  cairo_set_source(cr, pattern);
  cairo_paint(cr);
  cairo_destroy(cr);

  return FALSE;
}
