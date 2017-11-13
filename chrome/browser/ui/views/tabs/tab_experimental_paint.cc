// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_experimental_paint.h"

#include "cc/paint/paint_recorder.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"

namespace {

constexpr float kMinimumEndcapWidth = 4;

// Returns the width of the tab endcap in DIP.  More precisely, this is the
// width of the curve making up either the outer or inner edge of the stroke;
// since these two curves are horizontally offset by 1 px (regardless of scale),
// the total width of the endcap from tab outer edge to the inside end of the
// stroke inner edge is (GetUnscaledEndcapWidth() * scale) + 1.
//
// The value returned here must be at least kMinimumEndcapWidth.
float GetTabEndcapWidth() {
  return GetLayoutInsets(TAB).left() - 0.5f;
}

gfx::Size GetMinimumInactiveSize() {
  return gfx::Size(GetLayoutInsets(TAB).width(), GetLayoutConstant(TAB_HEIGHT));
}

float GetInverseDiagonalSlope() {
  // This is computed from the border path as follows:
  // * The endcap width is enough for the whole stroke outer curve, i.e. the
  //   side diagonal plus the curves on both its ends.
  // * The bottom and top curve together are kMinimumEndcapWidth DIP wide, so
  //   the diagonal is (endcap_width - kMinimumEndcapWidth) DIP wide.
  // * The bottom and top curve are each 1.5 px high.  Additionally, there is an
  //   extra 1 px below the bottom curve and (scale - 1) px above the top curve,
  //   so the diagonal is ((height - 1.5 - 1.5) * scale - 1 - (scale - 1)) px
  //   high.  Simplifying this gives (height - 4) * scale px, or (height - 4)
  //   DIP.
  return (GetTabEndcapWidth() - kMinimumEndcapWidth) /
         (GetMinimumInactiveSize().height() - 4);
}

// Returns a path corresponding to the tab's content region inside the outer
// stroke.
gfx::Path GetFillPath(float scale, const gfx::Size& size, float endcap_width) {
  const float right = size.width() * scale;
  // The bottom of the tab needs to be pixel-aligned or else when we call
  // ClipPath with anti-aliasing enabled it can cause artifacts.
  const float bottom = std::ceil(size.height() * scale);

  gfx::Path fill;
  fill.moveTo(right - 1, bottom);
  fill.rCubicTo(-0.75 * scale, 0, -1.625 * scale, -0.5 * scale, -2 * scale,
                -1.5 * scale);
  fill.lineTo(right - 1 - (endcap_width - 2) * scale, 2.5 * scale);
  // Prevent overdraw in the center near minimum width (only happens if
  // scale < 2).  We could instead avoid this by increasing the tab inset
  // values, but that would shift all the content inward as well, unless we
  // then overlapped the content on the endcaps, by which point we'd have a
  // huge mess.
  const float scaled_endcap_width = 1 + endcap_width * scale;
  const float overlap = scaled_endcap_width * 2 - right;
  const float offset = (overlap > 0) ? (overlap / 2) : 0;
  fill.rCubicTo(-0.375 * scale, -1 * scale, -1.25 * scale + offset,
                -1.5 * scale, -2 * scale + offset, -1.5 * scale);
  if (overlap < 0)
    fill.lineTo(scaled_endcap_width, scale);
  fill.rCubicTo(-0.75 * scale, 0, -1.625 * scale - offset, 0.5 * scale,
                -2 * scale - offset, 1.5 * scale);
  fill.lineTo(1 + 2 * scale, bottom - 1.5 * scale);
  fill.rCubicTo(-0.375 * scale, scale, -1.25 * scale, 1.5 * scale, -2 * scale,
                1.5 * scale);
  fill.close();
  return fill;
}

// Returns a path corresponding to the tab's outer border for a given tab
// |size|, |scale|, and |endcap_width|.  If |unscale_at_end| is true, this path
// will be normalized to a 1x scale by scaling by 1/scale before returning.  If
// |extend_to_top| is true, the path is extended vertically to the top of the
// tab bounds.  The caller uses this for Fitts' Law purposes in
// maximized/fullscreen mode.
gfx::Path GetBorderPath(float scale,
                        bool unscale_at_end,
                        bool extend_to_top,
                        float endcap_width,
                        const gfx::Size& size) {
  const float top = scale - 1;
  const float right = size.width() * scale;
  const float bottom = size.height() * scale;

  gfx::Path path;
  path.moveTo(0, bottom);
  path.rLineTo(0, -1);
  path.rCubicTo(0.75 * scale, 0, 1.625 * scale, -0.5 * scale, 2 * scale,
                -1.5 * scale);
  path.lineTo((endcap_width - 2) * scale, top + 1.5 * scale);
  if (extend_to_top) {
    // Create the vertical extension by extending the side diagonals until
    // they reach the top of the bounds.
    const float dy = 2.5 * scale - 1;
    const float dx = GetInverseDiagonalSlope() * dy;
    path.rLineTo(dx, -dy);
    path.lineTo(right - (endcap_width - 2) * scale - dx, 0);
    path.rLineTo(dx, dy);
  } else {
    path.rCubicTo(0.375 * scale, -scale, 1.25 * scale, -1.5 * scale, 2 * scale,
                  -1.5 * scale);
    path.lineTo(right - endcap_width * scale, top);
    path.rCubicTo(0.75 * scale, 0, 1.625 * scale, 0.5 * scale, 2 * scale,
                  1.5 * scale);
  }
  path.lineTo(right - 2 * scale, bottom - 1 - 1.5 * scale);
  path.rCubicTo(0.375 * scale, scale, 1.25 * scale, 1.5 * scale, 2 * scale,
                1.5 * scale);
  path.rLineTo(0, 1);
  path.close();

  if (unscale_at_end && (scale != 1))
    path.transform(SkMatrix::MakeScale(1.f / scale));

  return path;
}

}  // namespace

// TabExperimentalPaint::BackgroundCache ---------------------------------------

TabExperimentalPaint::BackgroundCache::BackgroundCache() = default;
TabExperimentalPaint::BackgroundCache::~BackgroundCache() = default;

bool TabExperimentalPaint::BackgroundCache::CacheKeyMatches(
    float scale,
    const gfx::Size& size,
    SkColor active_color,
    SkColor inactive_color,
    SkColor stroke_color) {
  return scale_ == scale && size_ == size && active_color_ == active_color &&
         inactive_color_ == inactive_color && stroke_color_ == stroke_color;
}

void TabExperimentalPaint::BackgroundCache::SetCacheKey(float scale,
                                                        const gfx::Size& size,
                                                        SkColor active_color,
                                                        SkColor inactive_color,
                                                        SkColor stroke_color) {
  scale_ = scale;
  size_ = size;
  active_color_ = active_color;
  inactive_color_ = inactive_color;
  stroke_color_ = stroke_color;
}

// TabExperimentalPaint --------------------------------------------------------

TabExperimentalPaint::TabExperimentalPaint(views::View* view) : view_(view) {}
TabExperimentalPaint::~TabExperimentalPaint() {}

void TabExperimentalPaint::PaintTabBackground(gfx::Canvas* canvas,
                                              bool active,
                                              int fill_id,
                                              int y_offset,
                                              gfx::Path* clip) {
  // |y_offset| is only set when |fill_id| is being used.
  DCHECK(!y_offset || fill_id);

  const float endcap_width = GetTabEndcapWidth();
  const ui::ThemeProvider* tp = view_->GetThemeProvider();
  const SkColor active_color = tp->GetColor(ThemeProperties::COLOR_TOOLBAR);
  const SkColor inactive_color =
      tp->GetColor(ThemeProperties::COLOR_BACKGROUND_TAB);
  const SkColor stroke_color =
      0xFFFF;  // controller_->GetToolbarTopSeparatorColor();
  const bool paint_hover_effect =
      false;  //! active && hover_controller_.ShouldDraw();

  // If there is a |fill_id| we don't try to cache. This could be improved
  // but would require knowing then the image from the ThemeProvider had been
  // changed, and invalidating when the tab's x-coordinate or background_offset_
  // changed.
  // Similarly, if |paint_hover_effect|, we don't try to cache since hover
  // effects change on every invalidation and we would need to invalidate the
  // cache based on the hover states.
  if (fill_id || paint_hover_effect) {
    gfx::Path fill_path =
        GetFillPath(canvas->image_scale(), view_->size(), endcap_width);
    gfx::Path stroke_path = GetBorderPath(canvas->image_scale(), false, false,
                                          endcap_width, view_->size());
    PaintTabBackgroundFill(canvas, fill_path, active, paint_hover_effect,
                           active_color, inactive_color, fill_id, y_offset);
    gfx::ScopedCanvas scoped_canvas(clip ? canvas : nullptr);
    if (clip)
      canvas->sk_canvas()->clipPath(*clip, SkClipOp::kDifference, true);
    PaintTabBackgroundStroke(canvas, fill_path, stroke_path, active,
                             stroke_color);
    return;
  }

  BackgroundCache& cache =
      active ? background_active_cache_ : background_inactive_cache_;
  if (!cache.CacheKeyMatches(canvas->image_scale(), view_->size(), active_color,
                             inactive_color, stroke_color)) {
    gfx::Path fill_path =
        GetFillPath(canvas->image_scale(), view_->size(), endcap_width);
    gfx::Path stroke_path = GetBorderPath(canvas->image_scale(), false, false,
                                          endcap_width, view_->size());
    cc::PaintRecorder recorder;

    {
      gfx::Canvas cache_canvas(recorder.beginRecording(view_->size().width(),
                                                       view_->size().height()),
                               canvas->image_scale());
      PaintTabBackgroundFill(&cache_canvas, fill_path, active,
                             paint_hover_effect, active_color, inactive_color,
                             fill_id, y_offset);
      cache.fill_record = recorder.finishRecordingAsPicture();
    }
    {
      gfx::Canvas cache_canvas(recorder.beginRecording(view_->size().width(),
                                                       view_->size().height()),
                               canvas->image_scale());
      PaintTabBackgroundStroke(&cache_canvas, fill_path, stroke_path, active,
                               stroke_color);
      cache.stroke_record = recorder.finishRecordingAsPicture();
    }

    cache.SetCacheKey(canvas->image_scale(), view_->size(), active_color,
                      inactive_color, stroke_color);
  }

  canvas->sk_canvas()->drawPicture(cache.fill_record);
  gfx::ScopedCanvas scoped_canvas(clip ? canvas : nullptr);
  if (clip)
    canvas->sk_canvas()->clipPath(*clip, SkClipOp::kDifference, true);
  canvas->sk_canvas()->drawPicture(cache.stroke_record);
}

void TabExperimentalPaint::PaintTabBackgroundFill(gfx::Canvas* canvas,
                                                  const gfx::Path& fill_path,
                                                  bool active,
                                                  bool paint_hover_effect,
                                                  SkColor active_color,
                                                  SkColor inactive_color,
                                                  int fill_id,
                                                  int y_offset) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  canvas->ClipPath(fill_path, true);
  if (fill_id) {
    gfx::ScopedCanvas scale_scoper(canvas);
    canvas->sk_canvas()->scale(scale, scale);
    canvas->TileImageInt(*view_->GetThemeProvider()->GetImageSkiaNamed(fill_id),
                         view_->GetMirroredX() + background_offset_.x(),
                         y_offset, 0, 0, view_->width(), view_->height());
  } else {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(active ? active_color : inactive_color);
    canvas->DrawRect(gfx::ScaleToEnclosingRect(view_->GetLocalBounds(), scale),
                     flags);
  }
  /*  TODO(brettw) need to add hover effect.
  if (paint_hover_effect) {
    SkPoint hover_location(gfx::PointToSkPoint(hover_controller_.location()));
    hover_location.scale(SkFloatToScalar(scale));
    const SkScalar kMinHoverRadius = 16;
    const SkScalar radius =
        std::max(SkFloatToScalar(width() / 4.f), kMinHoverRadius);
DrawHighlight(canvas, hover_location, radius * scale,
              SkColorSetA(active_color, hover_controller_.GetAlpha()));
  }
  */
}

void TabExperimentalPaint::PaintTabBackgroundStroke(
    gfx::Canvas* canvas,
    const gfx::Path& fill_path,
    const gfx::Path& stroke_path,
    bool active,
    SkColor color) {
  gfx::ScopedCanvas scoped_canvas(canvas);
  const float scale = canvas->UndoDeviceScaleFactor();

  if (!active) {
    // Clip out the bottom line; this will be drawn for us by
    // TabStrip::PaintChildren().
    canvas->ClipRect(
        gfx::RectF(view_->width() * scale, view_->height() * scale - 1));
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(color);
  SkPath path;
  Op(stroke_path, fill_path, kDifference_SkPathOp, &path);
  canvas->DrawPath(path, flags);
}
