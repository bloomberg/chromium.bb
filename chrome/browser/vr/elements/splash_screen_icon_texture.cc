// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/splash_screen_icon_texture.h"

#include "ui/gfx/canvas.h"

namespace vr {

SplashScreenIconTexture::SplashScreenIconTexture() = default;

SplashScreenIconTexture::~SplashScreenIconTexture() = default;

void SplashScreenIconTexture::SetSplashScreenIconBitmap(
    const SkBitmap& bitmap) {
  splash_screen_icon_ = SkImage::MakeFromBitmap(bitmap);
  set_dirty();
}

void SplashScreenIconTexture::Draw(SkCanvas* sk_canvas,
                                   const gfx::Size& texture_size) {
  size_.set_width(texture_size.width());
  size_.set_height(texture_size.height());
  sk_canvas->drawImageRect(
      splash_screen_icon_,
      SkRect::MakeXYWH(0, 0, size_.width(), size_.height()), nullptr);
}

gfx::Size SplashScreenIconTexture::GetPreferredTextureSize(
    int maximum_width) const {
  return gfx::Size(maximum_width, maximum_width);
}

gfx::SizeF SplashScreenIconTexture::GetDrawnSize() const {
  return size_;
}

}  // namespace vr
