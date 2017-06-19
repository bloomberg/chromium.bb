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

UrlBar::UrlBar(int preferred_width,
               const base::Callback<void()>& back_button_callback,
               const base::Callback<void()>& security_icon_callback,
               const base::Callback<void(UiUnsupportedMode)>& failure_callback)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<UrlBarTexture>(false, failure_callback)),
      back_button_callback_(back_button_callback),
      security_icon_callback_(security_icon_callback) {}

UrlBar::~UrlBar() = default;

void UrlBar::UpdateTexture() {
  TexturedElement::UpdateTexture();
  last_update_time_ = last_begin_frame_time_;
}

UiTexture* UrlBar::GetTexture() const {
  return texture_.get();
}

void UrlBar::OnHoverEnter(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void UrlBar::OnHoverLeave() {
  OnStateUpdated(gfx::PointF(std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()));
}

void UrlBar::OnMove(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void UrlBar::OnButtonDown(const gfx::PointF& position) {
  if (texture_->HitsBackButton(position))
    down_ = true;
  else if (texture_->HitsSecurityRegion(position))
    security_region_down_ = true;
  OnStateUpdated(position);
}

void UrlBar::OnButtonUp(const gfx::PointF& position) {
  down_ = false;
  OnStateUpdated(position);
  if (can_go_back_ && texture_->HitsBackButton(position))
    back_button_callback_.Run();
  else if (security_region_down_ && texture_->HitsSecurityRegion(position))
    security_icon_callback_.Run();
  security_region_down_ = false;
}

bool UrlBar::HitTest(const gfx::PointF& position) const {
  return texture_->HitsUrlBar(position) || texture_->HitsBackButton(position);
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
  set_visible(enabled);
}

void UrlBar::SetURL(const GURL& gurl) {
  texture_->SetURL(gurl);
}

void UrlBar::SetHistoryButtonsEnabled(bool can_go_back) {
  can_go_back_ = can_go_back;
  texture_->SetHistoryButtonsEnabled(can_go_back_);
}

void UrlBar::SetSecurityInfo(security_state::SecurityLevel level,
                             bool malware) {
  texture_->SetSecurityInfo(level, malware);
}

void UrlBar::OnStateUpdated(const gfx::PointF& position) {
  const bool hovered = texture_->HitsBackButton(position);
  const bool pressed = hovered ? down_ : false;

  texture_->SetBackButtonHovered(hovered);
  texture_->SetBackButtonPressed(pressed);
  UpdateTexture();
}

}  // namespace vr_shell
