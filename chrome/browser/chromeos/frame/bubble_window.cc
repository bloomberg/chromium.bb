// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/bubble_window.h"

#include <gtk/gtk.h>

#include "chrome/browser/chromeos/frame/bubble_frame_view.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "views/window/non_client_view.h"

namespace chromeos {

bool IsInsideCircle(int x0, int y0, int x1, int y1, int r) {
  return (x0 - x1) * (x0 - x1) + (y0 - y1) * (y0 - y1) <= r * r;
}

void SetRegionUnionWithPoint(int i, int j, GdkRegion* region) {
  GdkRectangle rect = {i, j, 1, 1};
  gdk_region_union_with_rect(region, &rect);
}

// static
const SkColor BubbleWindow::kBackgroundColor = SK_ColorWHITE;

BubbleWindow::BubbleWindow(views::WindowDelegate* window_delegate)
    : views::WindowGtk(window_delegate) {
  MakeTransparent();
}

void BubbleWindow::Init(GtkWindow* parent, const gfx::Rect& bounds) {
  views::WindowGtk::Init(parent, bounds);

  // Turn on double buffering so that the hosted GtkWidgets does not
  // flash as in http://crosbug.com/9065.
  EnableDoubleBuffer(true);

  GdkColor background_color = gfx::SkColorToGdkColor(kBackgroundColor);
  gtk_widget_modify_bg(GetNativeView(), GTK_STATE_NORMAL, &background_color);

  // A work-around for http://crosbug.com/8538. All GdkWidnow of top-level
  // GtkWindow should participate _NET_WM_SYNC_REQUEST protocol and window
  // manager should only show the window after getting notified. And we
  // should only notify window manager after at least one paint is done.
  // TODO(xiyuan): Figure out the right fix.
  gtk_widget_realize(GetNativeView());
  gdk_window_set_back_pixmap(GetNativeView()->window, NULL, FALSE);
  gtk_widget_realize(window_contents());
  gdk_window_set_back_pixmap(window_contents()->window, NULL, FALSE);
}

void BubbleWindow::TrimMargins(int margin_left, int margin_right,
                               int margin_top, int margin_bottom,
                               int border_radius) {
  gfx::Size size = GetNonClientView()->GetPreferredSize();
  const int w = size.width();
  const int h = size.height();
  GdkRectangle rect0 = {0, border_radius, w - margin_right,
                        h - margin_bottom - 2 * border_radius};
  GdkRectangle rect1 = {border_radius, 0,
                        w - margin_right - 2 * border_radius,
                        h - margin_bottom};
  GdkRegion* region = gdk_region_rectangle(&rect0);
  gdk_region_union_with_rect(region, &rect1);

  // Top Left
  for (int i = 0; i < border_radius; ++i) {
    for (int j = 0; j < border_radius; ++j) {
      if (IsInsideCircle(i + 0.5, j + 0.5, border_radius, border_radius,
                         border_radius)) {
        SetRegionUnionWithPoint(i, j, region);
      }
    }
  }
  // Top Right
  for (int i = w - margin_right - border_radius - 1; i < w; ++i) {
    for (int j = 0; j < border_radius; ++j) {
      if (IsInsideCircle(i + 0.5, j + 0.5, w - margin_right - border_radius - 1,
                         border_radius, border_radius)) {
        SetRegionUnionWithPoint(i, j, region);
      }
    }
  }
  // Bottom Left
  for (int i = 0; i < border_radius; ++i) {
    for (int j = h - margin_bottom - border_radius - 1; j < h; ++j) {
      if (IsInsideCircle(i + 0.5, j + 0.5, border_radius,
                         h - margin_bottom - border_radius - 1,
                         border_radius)) {
        SetRegionUnionWithPoint(i, j, region);
      }
    }
  }
  // Bottom Right
  for (int i = w - margin_right - border_radius - 1; i < w; ++i) {
    for (int j = h - margin_bottom - border_radius - 1; j < h; ++j) {
      if (IsInsideCircle(i + 0.5, j + 0.5, w - margin_right - border_radius - 1,
                         h - margin_bottom - border_radius - 1,
                         border_radius)) {
        SetRegionUnionWithPoint(i, j, region);
      }
    }
  }

  gdk_window_shape_combine_region(window_contents()->window, region,
                                  margin_left, margin_top);
  gdk_region_destroy(region);
}

views::Window* BubbleWindow::Create(
    gfx::NativeWindow parent,
    const gfx::Rect& bounds,
    Style style,
    views::WindowDelegate* window_delegate) {
  BubbleWindow* window = new BubbleWindow(window_delegate);
  window->GetNonClientView()->SetFrameView(new BubbleFrameView(window, style));
  window->Init(parent, bounds);

  if (style == STYLE_XSHAPE) {
    const int kMarginLeft = 14;
    const int kMarginRight = 28;
    const int kMarginTop = 12;
    const int kMarginBottom = 28;
    const int kBorderRadius = 8;
    window->TrimMargins(kMarginLeft, kMarginRight, kMarginTop, kMarginBottom,
                        kBorderRadius);
  }

  return window;
}

}  // namespace chromeos
