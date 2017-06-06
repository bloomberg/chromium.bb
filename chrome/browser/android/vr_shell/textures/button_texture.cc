// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/textures/button_texture.h"

namespace vr_shell {

ButtonTexture::ButtonTexture() = default;

ButtonTexture::~ButtonTexture() = default;

void ButtonTexture::OnSetMode() {
  set_dirty();
}

void ButtonTexture::SetPressed(bool pressed) {
  if (pressed_ != pressed)
    set_dirty();
  pressed_ = pressed;
}

void ButtonTexture::SetHovered(bool hovered) {
  if (hovered_ != hovered)
    set_dirty();
  hovered_ = hovered;
}

}  // namespace vr_shell
