// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gfx/native_theme_linux.h"

#include "base/logging.h"
#include "gfx/size.h"
#include "gfx/rect.h"

namespace gfx {

unsigned int NativeThemeLinux::button_length_ = 14;
unsigned int NativeThemeLinux::scrollbar_width_ = 15;
unsigned int NativeThemeLinux::thumb_inactive_color_ = 0xf0ebe5;
unsigned int NativeThemeLinux::thumb_active_color_ = 0xfaf8f5;
unsigned int NativeThemeLinux::track_color_ = 0xe3ddd8;

#if !defined(OS_CHROMEOS)
// Chromeos has a different look.
// static
NativeThemeLinux* NativeThemeLinux::instance() {
  // The global NativeThemeLinux instance.
  static NativeThemeLinux s_native_theme;
  return &s_native_theme;
}
#endif

NativeThemeLinux::NativeThemeLinux() {
}

NativeThemeLinux::~NativeThemeLinux() {
}

gfx::Size NativeThemeLinux::GetSize(Part part) const {
  switch (part) {
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
      return gfx::Size(scrollbar_width_, button_length_);
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      return gfx::Size(button_length_, scrollbar_width_);
    case kScrollbarHorizontalThumb:
      // This matches Firefox on Linux.
      return gfx::Size(2 * scrollbar_width_, scrollbar_width_);
    case kScrollbarVerticalThumb:
      // This matches Firefox on Linux.
      return gfx::Size(scrollbar_width_, 2 * scrollbar_width_);
      break;
    case kScrollbarHorizontalTrack:
      return gfx::Size(0, scrollbar_width_);
    case kScrollbarVerticalTrack:
      return gfx::Size(scrollbar_width_, 0);
  }
  return gfx::Size();
}

void NativeThemeLinux::PaintArrowButton(
    skia::PlatformCanvas* canvas,
    const gfx::Rect& rect, Part direction, State state) {
  int widthMiddle, lengthMiddle;
  SkPaint paint;
  if (direction == kScrollbarUpArrow || direction == kScrollbarDownArrow) {
    widthMiddle = rect.width() / 2 + 1;
    lengthMiddle = rect.height() / 2 + 1;
  } else {
    lengthMiddle = rect.width() / 2 + 1;
    widthMiddle = rect.height() / 2 + 1;
  }

  // Calculate button color.
  SkScalar trackHSV[3];
  SkColorToHSV(track_color_, trackHSV);
  SkColor buttonColor = SaturateAndBrighten(trackHSV, 0, 0.2);
  SkColor backgroundColor = buttonColor;
  if (state == kPressed) {
    SkScalar buttonHSV[3];
    SkColorToHSV(buttonColor, buttonHSV);
    buttonColor = SaturateAndBrighten(buttonHSV, 0, -0.1);
  } else if (state == kHover) {
    SkScalar buttonHSV[3];
    SkColorToHSV(buttonColor, buttonHSV);
    buttonColor = SaturateAndBrighten(buttonHSV, 0, 0.05);
  }

  SkIRect skrect;
  skrect.set(rect.x(), rect.y(), rect.x() + rect.width(), rect.y()
      + rect.height());
  // Paint the background (the area visible behind the rounded corners).
  paint.setColor(backgroundColor);
  canvas->drawIRect(skrect, paint);

  // Paint the button's outline and fill the middle
  SkPath outline;
  switch (direction) {
    case kScrollbarUpArrow:
      outline.moveTo(rect.x() + 0.5, rect.y() + rect.height() + 0.5);
      outline.rLineTo(0, -(rect.height() - 2));
      outline.rLineTo(2, -2);
      outline.rLineTo(rect.width() - 5, 0);
      outline.rLineTo(2, 2);
      outline.rLineTo(0, rect.height() - 2);
      break;
    case kScrollbarDownArrow:
      outline.moveTo(rect.x() + 0.5, rect.y() - 0.5);
      outline.rLineTo(0, rect.height() - 2);
      outline.rLineTo(2, 2);
      outline.rLineTo(rect.width() - 5, 0);
      outline.rLineTo(2, -2);
      outline.rLineTo(0, -(rect.height() - 2));
      break;
    case kScrollbarRightArrow:
      outline.moveTo(rect.x() - 0.5, rect.y() + 0.5);
      outline.rLineTo(rect.width() - 2, 0);
      outline.rLineTo(2, 2);
      outline.rLineTo(0, rect.height() - 5);
      outline.rLineTo(-2, 2);
      outline.rLineTo(-(rect.width() - 2), 0);
      break;
    case kScrollbarLeftArrow:
      outline.moveTo(rect.x() + rect.width() + 0.5, rect.y() + 0.5);
      outline.rLineTo(-(rect.width() - 2), 0);
      outline.rLineTo(-2, 2);
      outline.rLineTo(0, rect.height() - 5);
      outline.rLineTo(2, 2);
      outline.rLineTo(rect.width() - 2, 0);
      break;
    default:
      break;
  }
  outline.close();

  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(buttonColor);
  canvas->drawPath(outline, paint);

  paint.setAntiAlias(true);
  paint.setStyle(SkPaint::kStroke_Style);
  SkScalar thumbHSV[3];
  SkColorToHSV(thumb_inactive_color_, thumbHSV);
  paint.setColor(OutlineColor(trackHSV, thumbHSV));
  canvas->drawPath(outline, paint);

  // If the button is disabled or read-only, the arrow is drawn with the
  // outline color.
  if (state != kDisabled)
    paint.setColor(SK_ColorBLACK);

  paint.setAntiAlias(false);
  paint.setStyle(SkPaint::kFill_Style);

  SkPath path;
  // The constants in this block of code are hand-tailored to produce good
  // looking arrows without anti-aliasing.
  switch (direction) {
    case kScrollbarUpArrow:
      path.moveTo(rect.x() + widthMiddle - 4, rect.y() + lengthMiddle + 2);
      path.rLineTo(7, 0);
      path.rLineTo(-4, -4);
      break;
    case kScrollbarDownArrow:
      path.moveTo(rect.x() + widthMiddle - 4, rect.y() + lengthMiddle - 3);
      path.rLineTo(7, 0);
      path.rLineTo(-4, 4);
      break;
    case kScrollbarRightArrow:
      path.moveTo(rect.x() + lengthMiddle - 3, rect.y() + widthMiddle - 4);
      path.rLineTo(0, 7);
      path.rLineTo(4, -4);
      break;
    case kScrollbarLeftArrow:
      path.moveTo(rect.x() + lengthMiddle + 1, rect.y() + widthMiddle - 5);
      path.rLineTo(0, 9);
      path.rLineTo(-4, -4);
      break;
    default:
      break;
  }
  path.close();

  canvas->drawPath(path, paint);
}

void NativeThemeLinux::Paint(skia::PlatformCanvas* canvas,
                             Part part,
                             State state,
                             const gfx::Rect& rect,
                             const ExtraParams& extra) {
  switch (part) {
    case kScrollbarDownArrow:
    case kScrollbarUpArrow:
    case kScrollbarLeftArrow:
    case kScrollbarRightArrow:
      PaintArrowButton(canvas, rect, part, state);
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      PaintThumb(canvas, part, state, rect);
      break;
    case kScrollbarHorizontalTrack:
    case kScrollbarVerticalTrack:
      PaintTrack(canvas, part, state, extra.scrollbar_track, rect);
      break;
  }
}

void NativeThemeLinux::PaintTrack(skia::PlatformCanvas* canvas,
                                  Part part,
                                  State state,
                                  const ScrollbarTrackExtraParams& extra_params,
                                  const gfx::Rect& rect) {
  SkPaint paint;
  SkIRect skrect;

  skrect.set(rect.x(), rect.y(), rect.right(), rect.bottom());
  SkScalar track_hsv[3];
  SkColorToHSV(track_color_, track_hsv);
  paint.setColor(SaturateAndBrighten(track_hsv, 0, 0));
  canvas->drawIRect(skrect, paint);

  SkScalar thumb_hsv[3];
  SkColorToHSV(thumb_inactive_color_, thumb_hsv);

  paint.setColor(OutlineColor(track_hsv, thumb_hsv));
  DrawBox(canvas, rect, paint);
}

void NativeThemeLinux::PaintThumb(skia::PlatformCanvas* canvas,
                                  Part part,
                                  State state,
                                  const gfx::Rect& rect) {
  const bool hovered = state == kHover;
  const int midx = rect.x() + rect.width() / 2;
  const int midy = rect.y() + rect.height() / 2;
  const bool vertical = part == kScrollbarVerticalThumb;

  SkScalar thumb[3];
  SkColorToHSV(hovered ? thumb_active_color_ : thumb_inactive_color_, thumb);

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
  SkColorToHSV(track_color_, track);
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

void NativeThemeLinux::DrawVertLine(SkCanvas* canvas,
                                    int x,
                                    int y1,
                                    int y2,
                                    const SkPaint& paint) const {
  SkIRect skrect;
  skrect.set(x, y1, x + 1, y2 + 1);
  canvas->drawIRect(skrect, paint);
}

void NativeThemeLinux::DrawHorizLine(SkCanvas* canvas,
                                     int x1,
                                     int x2,
                                     int y,
                                     const SkPaint& paint) const {
  SkIRect skrect;
  skrect.set(x1, y, x2 + 1, y + 1);
  canvas->drawIRect(skrect, paint);
}

void NativeThemeLinux::DrawBox(SkCanvas* canvas,
                               const gfx::Rect& rect,
                               const SkPaint& paint) const {
  const int right = rect.x() + rect.width() - 1;
  const int bottom = rect.y() + rect.height() - 1;
  DrawHorizLine(canvas, rect.x(), right, rect.y(), paint);
  DrawVertLine(canvas, right, rect.y(), bottom, paint);
  DrawHorizLine(canvas, rect.x(), right, bottom, paint);
  DrawVertLine(canvas, rect.x(), rect.y(), bottom, paint);
}

SkScalar NativeThemeLinux::Clamp(SkScalar value,
                                 SkScalar min,
                                 SkScalar max) const {
  return std::min(std::max(value, min), max);
}

SkColor NativeThemeLinux::SaturateAndBrighten(SkScalar* hsv,
                                              SkScalar saturate_amount,
                                              SkScalar brighten_amount) const {
  SkScalar color[3];
  color[0] = hsv[0];
  color[1] = Clamp(hsv[1] + saturate_amount, 0.0, 1.0);
  color[2] = Clamp(hsv[2] + brighten_amount, 0.0, 1.0);
  return SkHSVToColor(color);
}

SkColor NativeThemeLinux::OutlineColor(SkScalar* hsv1, SkScalar* hsv2) const {
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

void NativeThemeLinux::SetScrollbarColors(unsigned inactive_color,
                                          unsigned active_color,
                                          unsigned track_color) const {
  thumb_inactive_color_ = inactive_color;
  thumb_active_color_ = active_color;
  track_color_ = track_color;
}

}  // namespace gfx
