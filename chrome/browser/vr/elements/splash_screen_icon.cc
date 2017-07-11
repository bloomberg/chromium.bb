// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/splash_screen_icon.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/splash_screen_icon_texture.h"

namespace vr {

SplashScreenIcon::SplashScreenIcon(int preferred_width)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<SplashScreenIconTexture>()) {}

SplashScreenIcon::~SplashScreenIcon() = default;

void SplashScreenIcon::SetSplashScreenIconBitmap(const SkBitmap& bitmap) {
  texture_->SetSplashScreenIconBitmap(bitmap);
  UpdateTexture();
}

UiTexture* SplashScreenIcon::GetTexture() const {
  return texture_.get();
}

}  // namespace vr
