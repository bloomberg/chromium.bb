// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_LOADING_INDICATOR_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_LOADING_INDICATOR_TEXTURE_H_

#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "ui/gfx/geometry/size.h"

class SkCanvas;

namespace vr_shell {

class LoadingIndicatorTexture : public UiTexture {
 public:
  LoadingIndicatorTexture();
  ~LoadingIndicatorTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetLoading(bool loading);
  void SetLoadProgress(float progress);

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;
  gfx::SizeF size_;
  bool loading_ = false;
  float progress_ = 0;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_LOADING_INDICATOR_TEXTURE_H_
