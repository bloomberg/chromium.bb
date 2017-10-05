// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/text.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/text_texture.h"

namespace vr {

Text::Text(int maximum_width_pixels,
           float font_height_meters,
           float text_width_meters,
           const base::string16& text)
    : TexturedElement(maximum_width_pixels),
      texture_(base::MakeUnique<TextTexture>(text,
                                             font_height_meters,
                                             text_width_meters)) {}
Text::~Text() {}

void Text::SetColor(SkColor color) {
  texture_->SetColor(color);
}

UiTexture* Text::GetTexture() const {
  return texture_.get();
}

}  // namespace vr
