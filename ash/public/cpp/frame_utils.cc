// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/frame_utils.h"

#include "ash/public/cpp/ash_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/window/hit_test_utils.h"
#include "ui/views/window/non_client_view.h"

namespace ash {

int FrameBorderNonClientHitTest(views::NonClientFrameView* view,
                                const gfx::Point& point_in_widget) {
  gfx::Rect expanded_bounds = view->bounds();
  int outside_bounds = kResizeOutsideBoundsSize;

  if (view->GetWidget()->GetNativeWindow()->env()->is_touch_down())
    outside_bounds *= kResizeOutsideBoundsScaleForTouch;
  expanded_bounds.Inset(-outside_bounds, -outside_bounds);

  if (!expanded_bounds.Contains(point_in_widget))
    return HTNOWHERE;

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  views::Widget* widget = view->GetWidget();
  bool can_ever_resize = widget->widget_delegate()->CanResize();
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border = kResizeInsideBoundsSize;
  if (widget->IsMaximized() || widget->IsFullscreen()) {
    resize_border = 0;
    can_ever_resize = false;
  }
  int frame_component = view->GetHTComponentForFrame(
      point_in_widget, resize_border, resize_border, kResizeAreaCornerSize,
      kResizeAreaCornerSize, can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component =
      widget->client_view()->NonClientHitTest(point_in_widget);
  if (client_component != HTNOWHERE)
    return client_component;

  // Check if it intersects with children (frame caption button, back button,
  // etc.).
  int hit_test_component =
      views::GetHitTestComponent(widget->non_client_view(), point_in_widget);
  if (hit_test_component != HTNOWHERE)
    return hit_test_component;

  // Caption is a safe default.
  return HTCAPTION;
}

void PaintThemedFrame(gfx::Canvas* canvas,
                      const gfx::ImageSkia& frame_image,
                      const gfx::ImageSkia& frame_overlay_image,
                      SkColor background_color,
                      const gfx::Rect& bounds,
                      int image_inset_x,
                      int image_inset_y,
                      int alpha) {
  SkColor opaque_background_color =
      SkColorSetA(background_color, SK_AlphaOPAQUE);

  // When no images are used, just draw a color, with the animation |alpha|
  // applied.
  if (frame_image.isNull() && frame_overlay_image.isNull()) {
    // We use kPlus blending mode so that between the active and inactive
    // background colors, the result is 255 alpha (i.e. opaque).
    canvas->DrawColor(SkColorSetA(opaque_background_color, alpha),
                      SkBlendMode::kPlus);
    return;
  }

  // This handles the case where blending is required between one or more images
  // and the background color. In this case we use a SaveLayerWithFlags() call
  // to draw all 2-3 components into a single layer then apply the alpha to them
  // together.
  const bool blending_required =
      alpha < 0xFF || (!frame_image.isNull() && !frame_overlay_image.isNull());
  if (blending_required) {
    cc::PaintFlags flags;
    // We use kPlus blending mode so that between the active and inactive
    // background colors, the result is 255 alpha (i.e. opaque).
    flags.setBlendMode(SkBlendMode::kPlus);
    flags.setAlpha(alpha);
    canvas->SaveLayerWithFlags(flags);
  }

  // Images can be transparent and we expect the background color to be present
  // behind them. Here the |alpha| will be applied to the background color by
  // the SaveLayer call, so use |opaque_background_color|.
  canvas->DrawColor(opaque_background_color);
  if (!frame_image.isNull()) {
    canvas->TileImageInt(frame_image, image_inset_x, image_inset_y, 0, 0,
                         bounds.width(), bounds.height(), 1.0f,
                         SkShader::kRepeat_TileMode,
                         SkShader::kMirror_TileMode);
  }
  if (!frame_overlay_image.isNull())
    canvas->DrawImageInt(frame_overlay_image, 0, 0);

  if (blending_required)
    canvas->Restore();
}

}  // namespace ash
