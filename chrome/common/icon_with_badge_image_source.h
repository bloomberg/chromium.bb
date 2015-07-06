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
    Badge(std::string text, SkColor text_color, SkColor background_color);
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

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

  // The base icon to draw.
  gfx::Image icon_;

  // An optional badge to draw over the base icon.
  scoped_ptr<Badge> badge_;

  DISALLOW_COPY_AND_ASSIGN(IconWithBadgeImageSource);
};

#endif  // CHROME_COMMON_ICON_WITH_BADGE_IMAGE_SOURCE_H_
