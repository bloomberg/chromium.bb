// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_INSECURE_CONTENT_TRANSIENT_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_INSECURE_CONTENT_TRANSIENT_TEXTURE_H_

#include "chrome/browser/android/vr_shell/textures/ui_texture.h"

namespace vr_shell {

class InsecureContentTransientTexture : public UITexture {
 public:
  InsecureContentTransientTexture(int texture_handle, int texture_size);
  ~InsecureContentTransientTexture() override;

 private:
  void Draw(gfx::Canvas* canvas) override;
  void SetSize() override;

  int height_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_INSECURE_CONTENT_TRANSIENT_TEXTURE_H_
