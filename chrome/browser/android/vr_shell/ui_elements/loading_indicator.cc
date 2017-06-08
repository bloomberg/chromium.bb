// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/loading_indicator.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/loading_indicator_texture.h"

namespace vr_shell {

namespace {

static constexpr int kVisibilityTimeoutMs = 200;
}

LoadingIndicator::LoadingIndicator(int preferred_width)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<LoadingIndicatorTexture>()) {
  set_visible(false);
}

LoadingIndicator::~LoadingIndicator() = default;

UiTexture* LoadingIndicator::GetTexture() const {
  return texture_.get();
}

void LoadingIndicator::SetEnabled(bool enabled) {
  if (enabled_ == enabled)
    return;
  enabled_ = enabled;
  SetVisibility();
}

void LoadingIndicator::SetLoading(bool loading) {
  if (loading_ == loading)
    return;
  loading_ = loading;
  texture_->SetLoading(loading);
  texture_->SetLoadProgress(0);
  ResetVisibilityTimer();
  SetVisibility();
}

void LoadingIndicator::SetLoadProgress(float progress) {
  texture_->SetLoadProgress(progress);
}

void LoadingIndicator::ResetVisibilityTimer() {
  if (enabled_ && !loading_) {
    visibility_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kVisibilityTimeoutMs),
        this, &LoadingIndicator::SetVisibility);
  } else {
    visibility_timer_.Stop();
  }
}

void LoadingIndicator::SetVisibility() {
  set_visible(enabled_ && (loading_ || visibility_timer_.IsRunning()));
}

void LoadingIndicator::OnBeginFrame(const base::TimeTicks& ticks) {
  if (enabled_)
    UpdateTexture();
}

}  // namespace vr_shell
