// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_BUTTON_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_BUTTON_TEXTURE_H_

#include "chrome/browser/android/vr_shell/textures/ui_texture.h"

namespace vr_shell {

class ButtonTexture : public UiTexture {
 public:
  ButtonTexture();
  ~ButtonTexture() override;

  void SetPressed(bool pressed);
  bool pressed() const { return pressed_; }

  void SetHovered(bool hovered);
  bool hovered() const { return hovered_; }

 private:
  void OnSetMode() override;
  bool pressed_ = false;
  bool hovered_ = false;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_BUTTON_TEXTURE_H_
