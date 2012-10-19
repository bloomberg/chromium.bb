// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dip_util.h"

#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_conversions.h"

namespace content {
namespace {

float GetScaleForView(const RenderWidgetHostView* view) {
  return ui::GetScaleFactorScale(GetScaleFactorForView(view));
}

}  // namespace

ui::ScaleFactor GetScaleFactorForView(const RenderWidgetHostView* view) {
  return ui::GetScaleFactorForNativeView(view ? view->GetNativeView() : NULL);
}

gfx::Point ConvertPointToDIP(const RenderWidgetHostView* view,
                             const gfx::Point& point_in_pixel) {
  return gfx::ToFlooredPoint(
      point_in_pixel.Scale(1.0f / GetScaleForView(view)));
}

gfx::Size ConvertSizeToDIP(const RenderWidgetHostView* view,
                           const gfx::Size& size_in_pixel) {
  return gfx::ToFlooredSize(
      size_in_pixel.Scale(1.0f / GetScaleForView(view)));
}

gfx::Rect ConvertRectToDIP(const RenderWidgetHostView* view,
                           const gfx::Rect& rect_in_pixel) {
  float scale = 1.0f / GetScaleForView(view);
  return gfx::Rect(gfx::ToFlooredPoint(rect_in_pixel.origin().Scale(scale)),
                   gfx::ToFlooredSize(rect_in_pixel.size().Scale(scale)));
}

gfx::Point ConvertPointToPixel(const RenderWidgetHostView* view,
                               const gfx::Point& point_in_dip) {
  return gfx::ToFlooredPoint(point_in_dip.Scale(GetScaleForView(view)));
}

gfx::Size ConvertSizeToPixel(const RenderWidgetHostView* view,
                             const gfx::Size& size_in_dip) {
  return gfx::ToFlooredSize(size_in_dip.Scale(GetScaleForView(view)));
}

gfx::Rect ConvertRectToPixel(const RenderWidgetHostView* view,
                             const gfx::Rect& rect_in_dip) {
    float scale = GetScaleForView(view);
    return gfx::Rect(gfx::ToFlooredPoint(rect_in_dip.origin().Scale(scale)),
                     gfx::ToFlooredSize(rect_in_dip.size().Scale(scale)));
}

}  // namespace content
