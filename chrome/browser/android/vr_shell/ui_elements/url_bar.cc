// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/url_bar.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/url_bar_texture.h"

namespace vr_shell {

namespace {

// We will often get spammed with many updates. We will also get security and
// url updates out of sync. To address both these problems, we will hang onto
// dirtyness for |kUpdateDelay| before updating our texture to reduce visual
// churn.
constexpr int64_t kUpdateDelayMS = 50;

}  // namespace

UrlBar::UrlBar(int preferred_width)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<UrlBarTexture>()) {}

UrlBar::~UrlBar() = default;

void UrlBar::UpdateTexture() {
  TexturedElement::UpdateTexture();
  last_update_time_ = last_begin_frame_time_;
}

UiTexture* UrlBar::GetTexture() const {
  return texture_.get();
}

void UrlBar::OnHoverEnter(gfx::PointF position) {
  if (!texture_->SetDrawFlags(UrlBarTexture::FLAG_HOVER))
    return;
  UpdateTexture();
}

void UrlBar::OnHoverLeave() {
  if (!texture_->SetDrawFlags(0))
    return;
  UpdateTexture();
}

void UrlBar::OnButtonUp(gfx::PointF position) {
  back_button_callback_.Run();
}

void UrlBar::OnBeginFrame(const base::TimeTicks& begin_frame_time) {
  last_begin_frame_time_ = begin_frame_time;
  if (enabled_ && texture_->dirty()) {
    int64_t delta_ms = (begin_frame_time - last_update_time_).InMilliseconds();
    if (delta_ms > kUpdateDelayMS)
      UpdateTexture();
  }
}

void UrlBar::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

void UrlBar::SetURL(const GURL& gurl) {
  texture_->SetURL(gurl);
}

void UrlBar::SetSecurityLevel(int level) {
  texture_->SetSecurityLevel(level);
}

void UrlBar::SetBackButtonCallback(const base::Callback<void()>& callback) {
  back_button_callback_ = callback;
}

}  // namespace vr_shell
