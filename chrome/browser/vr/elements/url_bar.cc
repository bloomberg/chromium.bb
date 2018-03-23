// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/url_bar.h"

#include "chrome/browser/vr/elements/url_bar_texture.h"

namespace vr {

UrlBar::UrlBar(
    int preferred_width,
    const base::RepeatingCallback<void(UiUnsupportedMode)>& failure_callback)
    : TexturedElement(preferred_width),
      texture_(std::make_unique<UrlBarTexture>(failure_callback)),
      failure_callback_(failure_callback) {}

UrlBar::~UrlBar() = default;

UiTexture* UrlBar::GetTexture() const {
  return texture_.get();
}

void UrlBar::SetToolbarState(const ToolbarState& state) {
  texture_->SetToolbarState(state);
}

void UrlBar::SetColors(const UrlBarColors& colors) {
  texture_->SetColors(colors);
}

}  // namespace vr
