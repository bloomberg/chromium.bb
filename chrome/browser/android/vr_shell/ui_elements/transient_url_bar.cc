// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/transient_url_bar.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/url_bar_texture.h"

namespace vr_shell {

TransientUrlBar::TransientUrlBar(
    int preferred_width,
    const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<UrlBarTexture>(true, failure_callback)) {}

TransientUrlBar::~TransientUrlBar() = default;

UiTexture* TransientUrlBar::GetTexture() const {
  return texture_.get();
}

void TransientUrlBar::SetURL(const GURL& gurl) {
  texture_->SetURL(gurl);
  UpdateTexture();
}

void TransientUrlBar::SetSecurityLevel(security_state::SecurityLevel level) {
  texture_->SetSecurityLevel(level);
  UpdateTexture();
}

}  // namespace vr_shell
