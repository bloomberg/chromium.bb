// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr_shell/ui_elements/close_button.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/vr_shell/textures/close_button_texture.h"

namespace vr_shell {

CloseButton::CloseButton(base::Callback<void()> click_handler)
    : TexturedElement(256),
      texture_(base::MakeUnique<CloseButtonTexture>()),
      click_handler_(click_handler) {}

CloseButton::~CloseButton() = default;

void CloseButton::OnHoverEnter(gfx::PointF position) {
  hover_ = true;
  OnStateUpdated();
}

void CloseButton::OnHoverLeave() {
  hover_ = false;
  OnStateUpdated();
}

void CloseButton::OnButtonDown(gfx::PointF position) {
  down_ = true;
  OnStateUpdated();
}

void CloseButton::OnButtonUp(gfx::PointF position) {
  down_ = false;
  OnStateUpdated();
  if (position.x() < 0 || position.x() > 1.0f)
    return;
  if (position.y() < 0 || position.y() > 1.0f)
    return;
  click_handler_.Run();
}

UiTexture* CloseButton::GetTexture() const {
  return texture_.get();
}

void CloseButton::OnStateUpdated() {
  int flags = hover_ ? CloseButtonTexture::FLAG_HOVER : 0;
  flags |= (down_ && hover_) ? CloseButtonTexture::FLAG_DOWN : 0;
  if (!texture_->SetDrawFlags(flags))
    return;
  UpdateTexture();
}

}  // namespace vr_shell
