// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_CLOSE_BUTTON_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_CLOSE_BUTTON_TEXTURE_H_

#include "chrome/browser/android/vr_shell/textures/button_texture.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr_shell {

class CloseButtonTexture : public ButtonTexture {
 public:
  CloseButtonTexture();
  ~CloseButtonTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;
  bool HitTest(const gfx::PointF& point) const override;

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_CLOSE_BUTTON_TEXTURE_H_
