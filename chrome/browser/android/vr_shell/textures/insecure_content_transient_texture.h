// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_INSECURE_CONTENT_TRANSIENT_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_INSECURE_CONTENT_TRANSIENT_TEXTURE_H_

#include "chrome/browser/android/vr_shell/textures/ui_texture.h"

namespace vr_shell {

class InsecureContentTransientTexture : public UiTexture {
 public:
  InsecureContentTransientTexture();
  ~InsecureContentTransientTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_INSECURE_CONTENT_TRANSIENT_TEXTURE_H_
