// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_

#include "chrome/browser/android/vr_shell/textures/ui_texture.h"

#include "url/gurl.h"

namespace vr_shell {

class UrlBarTexture : public UiTexture {
 public:
  UrlBarTexture();
  ~UrlBarTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetHover(bool hover);
  void SetURL(const GURL& gurl);

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;
  float ToPixels(float meters) const;

  gfx::SizeF size_;
  bool hover_ = false;
  GURL gurl_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_
