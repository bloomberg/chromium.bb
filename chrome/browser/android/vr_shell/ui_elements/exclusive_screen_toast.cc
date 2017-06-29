// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/exclusive_screen_toast.h"

#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

namespace vr_shell {

ExclusiveScreenToast::ExclusiveScreenToast(int preferred_width)
    : SimpleTexturedElement(preferred_width) {}

ExclusiveScreenToast::~ExclusiveScreenToast() = default;

void ExclusiveScreenToast::UpdateElementSize() {
  // Adjust the width of this element according to the texture. The width is
  // decided by the length of the text to show.
  gfx::SizeF drawn_size = GetTexture()->GetDrawnSize();
  float width = drawn_size.width() / drawn_size.height() * size().y();
  set_size({width, size().y(), 1});
}

}  // namespace vr_shell
