// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dip_util.h"

#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"

namespace content {

float GetDIPScaleFactor(const RenderWidgetHostView* view) {
  if (gfx::Screen::IsDIPEnabled()) {
    gfx::Display display = gfx::Screen::GetMonitorNearestWindow(
        view ? view->GetNativeView() : NULL);
    return display.device_scale_factor();
  }
  return 1.0f;
}

gfx::Point ConvertPointToDIP(const RenderWidgetHostView* view,
                             const gfx::Point& point_in_pixel) {
  return point_in_pixel.Scale(1.0f / GetDIPScaleFactor(view));
}

gfx::Size ConvertSizeToDIP(const RenderWidgetHostView* view,
                           const gfx::Size& size_in_pixel) {
  return size_in_pixel.Scale(1.0f / GetDIPScaleFactor(view));
}

gfx::Rect ConvertRectToDIP(const RenderWidgetHostView* view,
                           const gfx::Rect& rect_in_pixel) {
  float scale = 1.0f / GetDIPScaleFactor(view);
  return gfx::Rect(rect_in_pixel.origin().Scale(scale),
                   rect_in_pixel.size().Scale(scale));
}

gfx::Point ConvertPointToPixel(const RenderWidgetHostView* view,
                               const gfx::Point& point_in_dip) {
  return point_in_dip.Scale(GetDIPScaleFactor(view));
}

gfx::Size ConvertSizeToPixel(const RenderWidgetHostView* view,
                             const gfx::Size& size_in_dip) {
  return size_in_dip.Scale(GetDIPScaleFactor(view));
}

gfx::Rect ConvertRectToPixel(const RenderWidgetHostView* view,
                             const gfx::Rect& rect_in_dip) {
    float scale = GetDIPScaleFactor(view);
    return gfx::Rect(rect_in_dip.origin().Scale(scale),
                     rect_in_dip.size().Scale(scale));
}

}  // namespace content
