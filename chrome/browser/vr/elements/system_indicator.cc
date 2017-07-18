// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/system_indicator.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/system_indicator_texture.h"

namespace vr {

SystemIndicator::SystemIndicator(int preferred_width,
                                 float height_meters,
                                 const gfx::VectorIcon& icon,
                                 int string_id)
    : TexturedElement(preferred_width), height_meters_(height_meters) {
  if (string_id != 0) {
    texture_ = base::MakeUnique<SystemIndicatorTexture>(icon, string_id);
  } else {
    texture_ = base::MakeUnique<SystemIndicatorTexture>(icon);
  }
}

void SystemIndicator::UpdateElementSize() {
  gfx::SizeF drawn_size = GetTexture()->GetDrawnSize();
  float width = height_meters_ * drawn_size.width() / drawn_size.height();
  SetSize(width, height_meters_);
}

SystemIndicator::~SystemIndicator() = default;

UiTexture* SystemIndicator::GetTexture() const {
  return texture_.get();
}

}  // namespace vr
