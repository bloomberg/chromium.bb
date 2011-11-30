// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_util.h"

#include "skia/ext/image_operations.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/rect.h"

namespace profiles {

const int kAvatarIconWidth = 38;
const int kAvatarIconHeight = 31;

gfx::Image GetAvatarIconForMenu(const gfx::Image& image,
                                bool is_gaia_picture) {
  if (!is_gaia_picture)
    return image;

  int length = std::min(kAvatarIconWidth, kAvatarIconHeight) - 2;
  SkBitmap bmp = skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, length, length);
  gfx::CanvasSkia canvas(kAvatarIconWidth, kAvatarIconHeight, false);

  // Draw the icon centered on the canvas.
  int x = (kAvatarIconWidth - length) / 2;
  int y = (kAvatarIconHeight - length) / 2;
  canvas.DrawBitmapInt(bmp, x, y);

  // Draw a gray border on the inside of the icon.
  SkColor color = SkColorSetARGB(83, 0, 0, 0);
  canvas.DrawRectInt(color, x, y, length - 1, length - 1);

  return gfx::Image(new SkBitmap(canvas.ExtractBitmap()));
}

gfx::Image GetAvatarIconForTitleBar(const gfx::Image& image,
                                    bool is_gaia_picture,
                                    int dst_width,
                                    int dst_height) {
  if (!is_gaia_picture)
    return image;

  int length = std::min(std::min(kAvatarIconWidth, kAvatarIconHeight),
      std::min(dst_width, dst_height)) - 2;
  SkBitmap bmp = skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST, length, length);
  gfx::CanvasSkia canvas(dst_width, dst_height, false);

  // Draw the icon on the bottom center of the canvas.
  int x1 = (dst_width - length) / 2;
  int x2 = x1 + length;
  int y1 = dst_height - length - 1;
  int y2 = y1 + length;
  canvas.DrawBitmapInt(bmp, x1, y1);

  // Give the icon an etched look by drawing a highlight on the bottom edge
  // and a shadow on the remaining edges.
  SkColor highlight_color = SkColorSetARGB(128, 255, 255, 255);
  SkColor shadow_color = SkColorSetARGB(83, 0, 0, 0);
  // Bottom highlight.
  canvas.DrawLineInt(highlight_color, x1, y2 - 1, x2, y2 - 1);
  // Top shadow.
  canvas.DrawLineInt(shadow_color, x1, y1, x2, y1);
  // Left shadow.
  canvas.DrawLineInt(shadow_color, x1, y1 + 1, x1, y2 - 1);
  // Right shadow.
  canvas.DrawLineInt(shadow_color, x2 - 1, y1 + 1, x2 - 1, y2 - 1);

  return gfx::Image(new SkBitmap(canvas.ExtractBitmap()));
}

} // namespace
