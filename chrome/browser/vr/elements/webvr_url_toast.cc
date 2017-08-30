// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/webvr_url_toast.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/webvr_url_toast_texture.h"
#include "chrome/browser/vr/target_property.h"

namespace vr {

WebVrUrlToast::WebVrUrlToast(
    int preferred_width,
    const base::TimeDelta& timeout,
    const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<WebVrUrlToastTexture>(failure_callback)),
      transience_(this, timeout) {
  SetTransitionedProperties({OPACITY});
}

WebVrUrlToast::~WebVrUrlToast() = default;

UiTexture* WebVrUrlToast::GetTexture() const {
  return texture_.get();
}

void WebVrUrlToast::SetVisible(bool visible) {
  transience_.SetVisible(visible);
}

void WebVrUrlToast::SetToolbarState(const ToolbarState& state) {
  texture_->SetToolbarState(state);
}

}  // namespace vr
