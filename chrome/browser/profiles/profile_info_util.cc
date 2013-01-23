// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_info_util.h"

#include "skia/ext/image_operations.h"
#include "ui/gfx/canvas.h"
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
      *image.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST, length, length);
  gfx::Canvas canvas(gfx::Size(kAvatarIconWidth, kAvatarIconHeight),
                     ui::SCALE_FACTOR_100P, false);

  // Draw the icon centered on the canvas.
  int x = (kAvatarIconWidth - length) / 2;
  int y = (kAvatarIconHeight - length) / 2;
  canvas.DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(bmp), x, y);

  // Draw a gray border on the inside of the icon.
  SkColor color = SkColorSetARGB(83, 0, 0, 0);
  canvas.DrawRect(gfx::Rect(x, y, length - 1, length - 1), color);

  return gfx::Image(gfx::ImageSkia(canvas.ExtractImageRep()));
}

gfx::Image GetAvatarIconForWebUI(const gfx::Image& image,
                                 bool is_gaia_picture) {
  if (!is_gaia_picture)
    return image;

  int length = std::min(kAvatarIconWidth, kAvatarIconHeight) - 2;
  SkBitmap bmp = skia::ImageOperations::Resize(
      *image.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST, length, length);
  gfx::Canvas canvas(gfx::Size(kAvatarIconWidth, kAvatarIconHeight),
                     ui::SCALE_FACTOR_100P, false);

  // Draw the icon centered on the canvas.
  int x = (kAvatarIconWidth - length) / 2;
  int y = (kAvatarIconHeight - length) / 2;
  canvas.DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(bmp), x, y);

  return gfx::Image(gfx::ImageSkia(canvas.ExtractImageRep()));
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
      *image.ToSkBitmap(), skia::ImageOperations::RESIZE_BEST, length, length);
  gfx::Canvas canvas(gfx::Size(dst_width, dst_height), ui::SCALE_FACTOR_100P,
                     false);

  // Draw the icon on the bottom center of the canvas.
  int x1 = (dst_width - length) / 2;
  int x2 = x1 + length;
  int y1 = dst_height - length - 1;
  int y2 = y1 + length;
  canvas.DrawImageInt(gfx::ImageSkia::CreateFrom1xBitmap(bmp), x1, y1);

  // Give the icon an etched look by drawing a highlight on the bottom edge
  // and a shadow on the remaining edges.
  SkColor highlight_color = SkColorSetARGB(128, 255, 255, 255);
  SkColor shadow_color = SkColorSetARGB(83, 0, 0, 0);
  // Bottom highlight.
  canvas.DrawLine(gfx::Point(x1, y2 - 1), gfx::Point(x2, y2 - 1),
                  highlight_color);
  // Top shadow.
  canvas.DrawLine(gfx::Point(x1, y1), gfx::Point(x2, y1), shadow_color);
  // Left shadow.
  canvas.DrawLine(gfx::Point(x1, y1 + 1), gfx::Point(x1, y2 - 1), shadow_color);
  // Right shadow.
  canvas.DrawLine(gfx::Point(x2 - 1, y1 + 1), gfx::Point(x2 - 1, y2 - 1),
                  shadow_color);

  return gfx::Image(gfx::ImageSkia(canvas.ExtractImageRep()));
}

} // namespace
