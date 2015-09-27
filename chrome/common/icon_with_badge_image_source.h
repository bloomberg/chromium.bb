// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_ICON_WITH_BADGE_IMAGE_SOURCE_H_
#define CHROME_COMMON_ICON_WITH_BADGE_IMAGE_SOURCE_H_

#include <string>

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"

namespace gfx {
class Size;
}

// CanvasImageSource for creating extension icon with a badge.
// TODO(devlin): This class and its buddy badge_util don't really belong in
// chrome/common/.
class IconWithBadgeImageSource : public gfx::CanvasImageSource {
 public:
  // The data representing a badge to be painted over the base image.
  struct Badge {
    Badge(const std::string& text,
          SkColor text_color,
          SkColor background_color);
    ~Badge();

    std::string text;
    SkColor text_color;
    SkColor background_color;

   private:
    DISALLOW_COPY_AND_ASSIGN(Badge);
  };

  explicit IconWithBadgeImageSource(const gfx::Size& size);
  ~IconWithBadgeImageSource() override;

  void SetIcon(const gfx::Image& icon);
  void SetBadge(scoped_ptr<Badge> badge);
  void set_grayscale(bool grayscale) { grayscale_ = grayscale; }
  void set_paint_decoration(bool paint_decoration) {
    paint_decoration_ = paint_decoration;
  }

  bool grayscale() const { return grayscale_; }
  bool paint_decoration() const { return paint_decoration_; }

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

  // Paints a decoration over the base icon to indicate that the action wants to
  // run.
  void PaintDecoration(gfx::Canvas* canvas);

  // The base icon to draw.
  gfx::Image icon_;

  // An optional badge to draw over the base icon.
  scoped_ptr<Badge> badge_;

  // Whether or not the icon should be grayscaled (e.g., to show it is
  // disabled).
  bool grayscale_;

  // Whether or not to paint a decoration over the base icon to indicate the
  // represented action wants to run.
  bool paint_decoration_;

  DISALLOW_COPY_AND_ASSIGN(IconWithBadgeImageSource);
};

#endif  // CHROME_COMMON_ICON_WITH_BADGE_IMAGE_SOURCE_H_
