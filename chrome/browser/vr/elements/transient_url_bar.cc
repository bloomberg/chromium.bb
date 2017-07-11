// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transient_url_bar.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/url_bar_texture.h"

namespace vr {

TransientUrlBar::TransientUrlBar(
    int preferred_width,
    const base::TimeDelta& timeout,
    const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<UrlBarTexture>(true, failure_callback)),
      transience_(this, timeout) {}

TransientUrlBar::~TransientUrlBar() = default;

UiTexture* TransientUrlBar::GetTexture() const {
  return texture_.get();
}

void TransientUrlBar::SetEnabled(bool enabled) {
  transience_.SetEnabled(enabled);
}

void TransientUrlBar::SetToolbarState(const ToolbarState& state) {
  texture_->SetToolbarState(state);
}

}  // namespace vr
