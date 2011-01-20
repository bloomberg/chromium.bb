// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/native_theme_chromeos.h"

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "gfx/insets.h"
#include "gfx/rect.h"
#include "gfx/size.h"
#include "gfx/skbitmap_operations.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkShader.h"

/* static */
gfx::NativeThemeLinux* gfx::NativeThemeLinux::instance() {
  // The global NativeThemeChromeos instance.
  static NativeThemeChromeos s_native_theme;
  return &s_native_theme;
}

NativeThemeChromeos::NativeThemeChromeos() {
}

NativeThemeChromeos::~NativeThemeChromeos() {
}

gfx::Size NativeThemeChromeos::GetPartSize(Part part) const {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int scrollbar_width = rb.GetBitmapNamed(IDR_SCROLL_BACKGROUND)->width();
  int width = 0, height = 0;
  switch (part) {
    case kScrollbarUpArrow:
      width = scrollbar_width;
      height = rb.GetBitmapNamed(IDR_SCROLL_ARROW_UP)->height();
      break;
    case kScrollbarDownArrow:
      width = scrollbar_width;
      height = rb.GetBitmapNamed(IDR_SCROLL_ARROW_DOWN)->height();
      break;
    case kScrollbarLeftArrow:
      width = rb.GetBitmapNamed(IDR_SCROLL_ARROW_UP)->height();
      height = scrollbar_width;
      break;
    case kScrollbarRightArrow:
      width = rb.GetBitmapNamed(IDR_SCROLL_ARROW_DOWN)->height();
      height = scrollbar_width;
      break;
    case kScrollbarHorizontalTrack:
      width = 0;
      height = scrollbar_width;
      break;
    case kScrollbarVerticalTrack:
      width = scrollbar_width;
      height = 0;
      break;
    case kScrollbarHorizontalThumb:
    case kScrollbarVerticalThumb:
      // allow thumb to be square but no shorter.
      width = scrollbar_width;
      height = scrollbar_width;
      break;
    default:
      return NativeThemeLinux::GetPartSize(part);
  }
  return gfx::Size(width, height);
}

void NativeThemeChromeos::PaintScrollbarTrack(skia::PlatformCanvas* canvas,
    Part part, State state,
    const ScrollbarTrackExtraParams& extra_params, const gfx::Rect& rect) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (part == kScrollbarVerticalTrack) {
    SkBitmap* background =
        rb.GetBitmapNamed(IDR_SCROLL_BACKGROUND);
    SkBitmap* border_up =
        rb.GetBitmapNamed(IDR_SCROLL_BACKGROUND_BORDER_UP);
    SkBitmap* border_down =
        rb.GetBitmapNamed(IDR_SCROLL_BACKGROUND_BORDER_DOWN);
    // Draw track background.
    DrawBitmapInt(
        canvas, *background,
        0, 0, background->width(), 1,
        rect.x(), rect.y(), rect.width(), rect.height());
    // Draw up button lower border.
    canvas->drawBitmap(*border_up, extra_params.track_x, extra_params.track_y);
    // Draw down button upper border.
    canvas->drawBitmap(
        *border_down,
        extra_params.track_x,
        extra_params.track_y + extra_params.track_height - border_down->height()
        );
  } else {
    SkBitmap* background =
        GetHorizontalBitmapNamed(IDR_SCROLL_BACKGROUND);
    SkBitmap* border_left =
        GetHorizontalBitmapNamed(IDR_SCROLL_BACKGROUND_BORDER_UP);
    SkBitmap* border_right =
        GetHorizontalBitmapNamed(IDR_SCROLL_BACKGROUND_BORDER_DOWN);
    // Draw track background.
    DrawBitmapInt(
        canvas, *background,
        0, 0, 1, background->height(),
        rect.x(), rect.y(), rect.width(), rect.height());
    // Draw left button right border.
    canvas->drawBitmap(*border_left,extra_params.track_x, extra_params.track_y);
    // Draw right button left border.
    canvas->drawBitmap(
        *border_right,
        extra_params.track_x + extra_params.track_width - border_right->width(),
        extra_params.track_y);
  }
}

void NativeThemeChromeos::PaintScrollbarThumb(skia::PlatformCanvas* canvas,
    Part part, State state, const gfx::Rect& rect) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int resource_id = IDR_SCROLL_THUMB;
  if (state == kHovered)
    resource_id++;
  else if (state == kPressed)
    resource_id += 2;
  if (part == kScrollbarVerticalThumb) {
    SkBitmap* bitmap = rb.GetBitmapNamed(resource_id);
    // Top
    DrawBitmapInt(
        canvas, *bitmap,
        0, 1, bitmap->width(), 5,
        rect.x(), rect.y(), rect.width(), 5);
    // Middle
    DrawBitmapInt(
        canvas, *bitmap,
        0, 7, bitmap->width(), 1,
        rect.x(), rect.y() + 5, rect.width(), rect.height() - 10);
    // Bottom
    DrawBitmapInt(
        canvas, *bitmap,
        0, 8, bitmap->width(), 5,
        rect.x(), rect.y() + rect.height() - 5, rect.width(), 5);
  } else {
    SkBitmap* bitmap = GetHorizontalBitmapNamed(resource_id);
    // Left
    DrawBitmapInt(
        canvas, *bitmap,
        1, 0, 5, bitmap->height(),
        rect.x(), rect.y(), 5, rect.height());
    // Middle
    DrawBitmapInt(
        canvas, *bitmap,
        7, 0, 1, bitmap->height(),
        rect.x() + 5, rect.y(), rect.width() - 10, rect.height());
    // Right
    DrawBitmapInt(
        canvas, *bitmap,
        8, 0, 5, bitmap->height(),
        rect.x() + rect.width() - 5, rect.y(), 5, rect.height());
  }
}

void NativeThemeChromeos::PaintArrowButton(skia::PlatformCanvas* canvas,
    const gfx::Rect& rect, Part part, State state) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int resource_id =
      (part == kScrollbarUpArrow || part == kScrollbarLeftArrow) ?
          IDR_SCROLL_ARROW_UP : IDR_SCROLL_ARROW_DOWN;
  if (state == kHovered)
    resource_id++;
  else if (state == kPressed)
    resource_id += 2;
  SkBitmap* bitmap;
  if (part == kScrollbarUpArrow || part == kScrollbarDownArrow)
    bitmap = rb.GetBitmapNamed(resource_id);
  else
    bitmap = GetHorizontalBitmapNamed(resource_id);
  canvas->drawBitmap(*bitmap, rect.x(), rect.y());
}

SkBitmap* NativeThemeChromeos::GetHorizontalBitmapNamed(int resource_id) {
  SkImageMap::const_iterator found = horizontal_bitmaps_.find(resource_id);
  if (found != horizontal_bitmaps_.end())
    return found->second;

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  SkBitmap* vertical_bitmap = rb.GetBitmapNamed(resource_id);

  if (vertical_bitmap) {
    SkBitmap transposed_bitmap =
        SkBitmapOperations::CreateTransposedBtmap(*vertical_bitmap);
    SkBitmap* horizontal_bitmap = new SkBitmap(transposed_bitmap);

    horizontal_bitmaps_[resource_id] = horizontal_bitmap;
    return horizontal_bitmap;
  }
  return NULL;
}
