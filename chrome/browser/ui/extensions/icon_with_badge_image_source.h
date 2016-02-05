// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_

#include <string>

#include "base/macros.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"

namespace gfx {
class Size;
}

// CanvasImageSource for creating extension icon with a badge.
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
  void set_paint_page_action_decoration(bool should_paint) {
    paint_page_action_decoration_ = should_paint;
  }
  void set_paint_blocked_actions_decoration(bool should_paint) {
    paint_blocked_actions_decoration_ = should_paint;
  }

  bool grayscale() const { return grayscale_; }
  bool paint_page_action_decoration() const {
    return paint_page_action_decoration_;
  }
  bool paint_blocked_actions_decoration() const {
    return paint_blocked_actions_decoration_;
  }

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override;

  // Paints |badge_|, if any, on |canvas|.
  void PaintBadge(gfx::Canvas* canvas);

  // Paints a decoration over the base icon to indicate that the action wants to
  // run.
  void PaintPageActionDecoration(gfx::Canvas* canvas);

  // Paints a decoration over the base icon to indicate that the extension has
  // a blocked action that wants to run.
  void PaintBlockedActionDecoration(gfx::Canvas* canvas);

  // The base icon to draw.
  gfx::Image icon_;

  // An optional badge to draw over the base icon.
  scoped_ptr<Badge> badge_;

  // Whether or not the icon should be grayscaled (e.g., to show it is
  // disabled).
  bool grayscale_;

  // Whether or not to paint a decoration over the base icon to indicate the
  // represented action wants to run.
  bool paint_page_action_decoration_;

  // Whether or not to paint a decoration to indicate that the extension has
  // had actions blocked.
  bool paint_blocked_actions_decoration_;

  DISALLOW_COPY_AND_ASSIGN(IconWithBadgeImageSource);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_ICON_WITH_BADGE_IMAGE_SOURCE_H_
