// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper_scrollbar_widget.h"

#include "base/logging.h"
#include "chrome/renderer/pepper_devices.h"
#include "skia/ext/platform_canvas.h"

namespace {

unsigned int g_scrollbar_width = 15;
unsigned int g_thumb_inactive_color = 0xf0ebe5;
unsigned int g_thumb_active_color = 0xfaf8f5;
unsigned int g_track_color = 0xe3ddd8;

SkScalar Clamp(SkScalar value,
               SkScalar min,
               SkScalar max) {
  return std::min(std::max(value, min), max);
}

SkColor SaturateAndBrighten(SkScalar* hsv,
                            SkScalar saturate_amount,
                            SkScalar brighten_amount) {
  SkScalar color[3];
  color[0] = hsv[0];
  color[1] = Clamp(hsv[1] + saturate_amount, 0.0, 1.0);
  color[2] = Clamp(hsv[2] + brighten_amount, 0.0, 1.0);
  return SkHSVToColor(color);
}

SkColor OutlineColor(SkScalar* hsv1, SkScalar* hsv2) {
  // GTK Theme engines have way too much control over the layout of
  // the scrollbar. We might be able to more closely approximate its
  // look-and-feel, if we sent whole images instead of just colors
  // from the browser to the renderer. But even then, some themes
  // would just break.
  //
  // So, instead, we don't even try to 100% replicate the look of
  // the native scrollbar. We render our own version, but we make
  // sure to pick colors that blend in nicely with the system GTK
  // theme. In most cases, we can just sample a couple of pixels
  // from the system scrollbar and use those colors to draw our
  // scrollbar.
  //
  // This works fine for the track color and the overall thumb
  // color. But it fails spectacularly for the outline color used
  // around the thumb piece.  Not all themes have a clearly defined
  // outline. For some of them it is partially transparent, and for
  // others the thickness is very unpredictable.
  //
  // So, instead of trying to approximate the system theme, we
  // instead try to compute a reasonable looking choice based on the
  // known color of the track and the thumb piece. This is difficult
  // when trying to deal both with high- and low-contrast themes,
  // and both with positive and inverted themes.
  //
  // The following code has been tested to look OK with all of the
  // default GTK themes.
  SkScalar min_diff = Clamp((hsv1[1] + hsv2[1]) * 1.2, 0.2, 0.5);
  SkScalar diff = Clamp(fabs(hsv1[2] - hsv2[2]) / 2, min_diff, 0.5);

  if (hsv1[2] + hsv2[2] > 1.0)
    diff = -diff;

  return SaturateAndBrighten(hsv2, -0.2, diff);
}


void DrawVertLine(SkCanvas* canvas,
                  int x,
                  int y1,
                  int y2,
                  const SkPaint& paint) {
  SkIRect skrect;
  skrect.set(x, y1, x + 1, y2 + 1);
  canvas->drawIRect(skrect, paint);
}

void DrawHorizLine(SkCanvas* canvas,
                  int x1,
                  int x2,
                  int y,
                  const SkPaint& paint) {
  SkIRect skrect;
  skrect.set(x1, y, x2 + 1, y + 1);
  canvas->drawIRect(skrect, paint);
}

void DrawBox(SkCanvas* canvas,
             const gfx::Rect& rect,
             const SkPaint& paint) {
  const int right = rect.x() + rect.width() - 1;
  const int bottom = rect.y() + rect.height() - 1;
  DrawHorizLine(canvas, rect.x(), right, rect.y(), paint);
  DrawVertLine(canvas, right, rect.y(), bottom, paint);
  DrawHorizLine(canvas, rect.x(), right, bottom, paint);
  DrawVertLine(canvas, rect.x(), rect.y(), bottom, paint);
}

void PaintTrack(skia::PlatformCanvas* canvas,
                const gfx::Rect& rect) {
  SkPaint paint;
  SkIRect skrect;

  skrect.set(rect.x(), rect.y(), rect.right(), rect.bottom());
  SkScalar track_hsv[3];
  SkColorToHSV(g_track_color, track_hsv);
  paint.setColor(SaturateAndBrighten(track_hsv, 0, 0));
  canvas->drawIRect(skrect, paint);

  SkScalar thumb_hsv[3];
  SkColorToHSV(g_thumb_inactive_color, thumb_hsv);

  paint.setColor(OutlineColor(track_hsv, thumb_hsv));
  DrawBox(canvas, rect, paint);
}

void PaintThumb(skia::PlatformCanvas* canvas,
                bool vertical,
                bool hovered,
                const gfx::Rect& rect) {
  const int midx = rect.x() + rect.width() / 2;
  const int midy = rect.y() + rect.height() / 2;

  SkScalar thumb[3];
  SkColorToHSV(hovered ? g_thumb_active_color : g_thumb_inactive_color, thumb);

  SkPaint paint;
  paint.setColor(SaturateAndBrighten(thumb, 0, 0.02));

  SkIRect skrect;
  if (vertical)
    skrect.set(rect.x(), rect.y(), midx + 1, rect.y() + rect.height());
  else
    skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), midy + 1);

  canvas->drawIRect(skrect, paint);

  paint.setColor(SaturateAndBrighten(thumb, 0, -0.02));

  if (vertical) {
    skrect.set(
        midx + 1, rect.y(), rect.x() + rect.width(), rect.y() + rect.height());
  } else {
    skrect.set(
        rect.x(), midy + 1, rect.x() + rect.width(), rect.y() + rect.height());
  }

  canvas->drawIRect(skrect, paint);

  SkScalar track[3];
  SkColorToHSV(g_track_color, track);
  paint.setColor(OutlineColor(track, thumb));
  DrawBox(canvas, rect, paint);

  if (rect.height() > 10 && rect.width() > 10) {
    const int grippy_half_width = 2;
    const int inter_grippy_offset = 3;
    if (vertical) {
      DrawHorizLine(canvas,
                    midx - grippy_half_width,
                    midx + grippy_half_width,
                    midy - inter_grippy_offset,
                    paint);
      DrawHorizLine(canvas,
                    midx - grippy_half_width,
                    midx + grippy_half_width,
                    midy,
                    paint);
      DrawHorizLine(canvas,
                    midx - grippy_half_width,
                    midx + grippy_half_width,
                    midy + inter_grippy_offset,
                    paint);
    } else {
      DrawVertLine(canvas,
                   midx - inter_grippy_offset,
                   midy - grippy_half_width,
                   midy + grippy_half_width,
                   paint);
      DrawVertLine(canvas,
                   midx,
                   midy - grippy_half_width,
                   midy + grippy_half_width,
                   paint);
      DrawVertLine(canvas,
                   midx + inter_grippy_offset,
                   midy - grippy_half_width,
                   midy + grippy_half_width,
                   paint);
    }
  }
}


}  // annonymous namespace

void PepperScrollbarWidget::Paint(Graphics2DDeviceContext* context,
                                  const NPRect& dirty) {
  skia::PlatformCanvas* canvas = context->canvas();

  // Beginning track.
  PaintTrack(canvas, BackTrackRect());

  // End track.
  PaintTrack(canvas, ForwardTrackRect());

  // Thumb.
  PaintThumb(canvas, vertical_, IsThumb(hovered_part_), ThumbRect());

  canvas->endPlatformPaint();
}

void PepperScrollbarWidget::GenerateMeasurements() {
  // TODO(port)
  thickness_ = g_scrollbar_width;
  arrow_length_ = 0;  // On Linux, we don't use buttons.
}

bool PepperScrollbarWidget::ShouldSnapBack(const gfx::Point& location) const {
  return false;
}

bool PepperScrollbarWidget::ShouldCenterOnThumb(
    const NPPepperEvent& event) const {
  return (event.u.mouse.button == NPMouseButton_Left &&
          event.u.mouse.modifier & NPEventModifier_ShiftKey) ||
         event.u.mouse.button == NPMouseButton_Middle;
}

void PepperScrollbarWidget::SetScrollbarColors(unsigned inactive_color,
                                               unsigned active_color,
                                               unsigned track_color) {
  g_thumb_inactive_color = inactive_color;
  g_thumb_active_color = active_color;
  g_track_color = track_color;
}

int PepperScrollbarWidget::MinimumThumbLength() const {
  return 2 * g_scrollbar_width;  // This matches Firefox on Linux.
}
