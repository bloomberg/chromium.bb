// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_SPLASH_SCREEN_ICON_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_SPLASH_SCREEN_ICON_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "third_party/skia/include/core/SkImage.h"

namespace vr_shell {

class SplashScreenIconTexture : public UiTexture {
 public:
  SplashScreenIconTexture();
  ~SplashScreenIconTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetSplashScreenIconBitmap(const SkBitmap& bitmap);

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  sk_sp<SkImage> splash_screen_icon_;
  gfx::SizeF size_;

  DISALLOW_COPY_AND_ASSIGN(SplashScreenIconTexture);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_SPLASH_SCREEN_ICON_TEXTURE_H_
