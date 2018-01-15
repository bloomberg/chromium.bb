// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/webvr_url_toast.h"

#include "chrome/browser/vr/elements/webvr_url_toast_texture.h"
#include "chrome/browser/vr/target_property.h"

namespace vr {

WebVrUrlToast::WebVrUrlToast(
    int preferred_width,
    const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : TexturedElement(preferred_width),
      texture_(std::make_unique<WebVrUrlToastTexture>(failure_callback)) {}

WebVrUrlToast::~WebVrUrlToast() = default;

UiTexture* WebVrUrlToast::GetTexture() const {
  return texture_.get();
}

void WebVrUrlToast::SetToolbarState(const ToolbarState& state) {
  texture_->SetToolbarState(state);
}

}  // namespace vr
