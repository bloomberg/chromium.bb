// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/exclusive_screen_toast.h"

#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/ui_texture.h"

namespace vr {

ExclusiveScreenToast::ExclusiveScreenToast(int preferred_width)
    : SimpleTexturedElement(preferred_width) {}

ExclusiveScreenToast::~ExclusiveScreenToast() = default;

void ExclusiveScreenToast::UpdateElementSize() {
  // Adjust the width of this element according to the texture. The width is
  // decided by the length of the text to show.
  gfx::SizeF drawn_size = GetTexture()->GetDrawnSize();
  float width =
      drawn_size.width() / drawn_size.height() * stale_size().height();
  SetSize(width, stale_size().height());
}

}  // namespace vr
