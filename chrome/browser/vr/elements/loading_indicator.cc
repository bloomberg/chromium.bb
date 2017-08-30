// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/loading_indicator.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/loading_indicator_texture.h"

namespace vr {

LoadingIndicator::LoadingIndicator(int preferred_width)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<LoadingIndicatorTexture>()) {
  SetVisible(false);
}

LoadingIndicator::~LoadingIndicator() = default;

UiTexture* LoadingIndicator::GetTexture() const {
  return texture_.get();
}

void LoadingIndicator::SetVisible(bool visible) {
  if (visible_ == visible)
    return;
  visible_ = visible;
  UpdateOpacity();
}

void LoadingIndicator::SetLoading(bool loading) {
  if (loading_ == loading)
    return;
  loading_ = loading;
  texture_->SetLoading(loading);
  texture_->SetLoadProgress(0);
  UpdateOpacity();
}

void LoadingIndicator::SetLoadProgress(float progress) {
  texture_->SetLoadProgress(progress);
}

void LoadingIndicator::UpdateOpacity() {
  SetOpacity((visible_ && loading_) ? opacity_when_visible() : 0);
}

}  // namespace vr
