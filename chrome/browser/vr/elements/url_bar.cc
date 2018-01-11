// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/url_bar.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/url_bar_texture.h"

namespace vr {

UrlBar::UrlBar(
    int preferred_width,
    const base::RepeatingCallback<void()>& url_click_callback,
    const base::RepeatingCallback<void(UiUnsupportedMode)>& failure_callback)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<UrlBarTexture>(failure_callback)),
      url_click_callback_(url_click_callback),
      failure_callback_(failure_callback) {}

UrlBar::~UrlBar() = default;

UiTexture* UrlBar::GetTexture() const {
  return texture_.get();
}

void UrlBar::OnButtonDown(const gfx::PointF& position) {
  if (texture_->HitsSecurityRegion(position))
    security_region_down_ = true;
  else
    url_down_ = true;
}

void UrlBar::OnButtonUp(const gfx::PointF& position) {
  if (security_region_down_ && texture_->HitsSecurityRegion(position))
    failure_callback_.Run(UiUnsupportedMode::kUnhandledPageInfo);
  else
    url_click_callback_.Run();
  security_region_down_ = false;
  url_down_ = false;
}

void UrlBar::SetToolbarState(const ToolbarState& state) {
  texture_->SetToolbarState(state);
}

void UrlBar::SetColors(const UrlBarColors& colors) {
  texture_->SetColors(colors);
}

}  // namespace vr
