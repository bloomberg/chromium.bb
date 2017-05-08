// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/url_bar.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/url_bar_texture.h"

namespace vr_shell {

UrlBar::UrlBar(int preferred_width)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<UrlBarTexture>()) {}

UrlBar::~UrlBar() = default;

void UrlBar::OnHoverEnter() {
  texture_->SetHover(true);
  Update();
}

void UrlBar::OnHoverLeave() {
  texture_->SetHover(false);
  Update();
}

UiTexture* UrlBar::GetTexture() const {
  return texture_.get();
}

void UrlBar::SetURL(const GURL& gurl) {
  texture_->SetURL(gurl);
}

}  // namespace vr_shell
