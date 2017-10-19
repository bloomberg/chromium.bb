// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/button.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/button_texture.h"
#include "ui/gfx/geometry/point_f.h"

namespace vr {

Button::Button(base::Callback<void()> click_handler,
               std::unique_ptr<ButtonTexture> texture)
    : TexturedElement(256),
      texture_(std::move(texture)),
      click_handler_(click_handler) {}

Button::~Button() = default;

void Button::OnHoverEnter(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void Button::OnHoverLeave() {
  OnStateUpdated(gfx::PointF(std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()));
}

void Button::OnMove(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void Button::OnButtonDown(const gfx::PointF& position) {
  down_ = true;
  OnStateUpdated(position);
}

void Button::OnButtonUp(const gfx::PointF& position) {
  down_ = false;
  OnStateUpdated(position);
  if (HitTest(position))
    click_handler_.Run();
}

bool Button::HitTest(const gfx::PointF& point) const {
  return texture_->HitTest(point);
}

UiTexture* Button::GetTexture() const {
  return texture_.get();
}

void Button::OnStateUpdated(const gfx::PointF& position) {
  const bool hovered = HitTest(position);
  const bool pressed = hovered ? down_ : false;

  texture_->SetHovered(hovered);
  texture_->SetPressed(pressed);
}

}  // namespace vr
