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

void CloseButton::OnHoverEnter(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void CloseButton::OnHoverLeave() {
  OnStateUpdated(gfx::PointF(std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()));
}

void CloseButton::OnMove(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void CloseButton::OnButtonDown(const gfx::PointF& position) {
  down_ = true;
  OnStateUpdated(position);
}

void CloseButton::OnButtonUp(const gfx::PointF& position) {
  down_ = false;
  OnStateUpdated(position);
  if (HitTest(position))
    click_handler_.Run();
}

bool CloseButton::HitTest(const gfx::PointF& point) const {
  return (point - gfx::PointF(0.5, 0.5)).LengthSquared() < 0.25;
}

UiTexture* CloseButton::GetTexture() const {
  return texture_.get();
}

void CloseButton::OnStateUpdated(const gfx::PointF& position) {
  bool hitting = HitTest(position);
  bool down = hitting ? down_ : false;

  int flags = hitting ? CloseButtonTexture::FLAG_HOVER : 0;
  flags |= down ? CloseButtonTexture::FLAG_DOWN : 0;
  if (!texture_->SetDrawFlags(flags))
    return;
  UpdateTexture();
}

}  // namespace vr_shell
