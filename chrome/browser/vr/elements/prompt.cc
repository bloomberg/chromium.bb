// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/prompt.h"

#include "chrome/browser/vr/elements/prompt_texture.h"

namespace vr {

Prompt::Prompt(int preferred_width,
               int content_message_id,
               const gfx::VectorIcon& icon,
               int primary_button_message_id,
               int secondary_button_message_id,
               const PromptCallback& result_callback)
    : TexturedElement(preferred_width),
      texture_(std::make_unique<PromptTexture>(content_message_id,
                                               icon,
                                               primary_button_message_id,
                                               secondary_button_message_id)),
      result_callback_(result_callback) {}

Prompt::~Prompt() = default;

void Prompt::OnHoverEnter(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void Prompt::OnHoverLeave() {
  OnStateUpdated(gfx::PointF(std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()));
}

void Prompt::OnMove(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void Prompt::OnButtonDown(const gfx::PointF& position) {
  if (texture_->HitsPrimaryButton(position))
    primary_down_ = true;
  else if (texture_->HitsSecondaryButton(position))
    secondary_down_ = true;
  OnStateUpdated(position);
}

void Prompt::OnButtonUp(const gfx::PointF& position) {
  if (primary_down_ && texture_->HitsPrimaryButton(position))
    result_callback_.Run(PRIMARY, reason_);
  else if (secondary_down_ && texture_->HitsSecondaryButton(position))
    result_callback_.Run(SECONDARY, reason_);

  primary_down_ = false;
  secondary_down_ = false;

  OnStateUpdated(position);
}

void Prompt::SetContentMessageId(int message_id) {
  texture_->SetContentMessageId(message_id);
}

void Prompt::SetIconColor(SkColor color) {
  static_cast<PromptTexture*>(GetTexture())->SetIconColor(color);
}

void Prompt::SetPrimaryButtonColors(const ButtonColors& colors) {
  texture_->SetPrimaryButtonColors(colors);
}

void Prompt::SetSecondaryButtonColors(const ButtonColors& colors) {
  texture_->SetSecondaryButtonColors(colors);
}

void Prompt::SetTextureForTesting(std::unique_ptr<PromptTexture> texture) {
  texture_ = std::move(texture);
}

void Prompt::ClickPrimaryButtonForTesting() {
  result_callback_.Run(PRIMARY, reason_);
}

void Prompt::ClickSecondaryButtonForTesting() {
  result_callback_.Run(SECONDARY, reason_);
}

void Prompt::Cancel() {
  result_callback_.Run(NONE, reason_);
}

void Prompt::OnStateUpdated(const gfx::PointF& position) {
  const bool primary_hovered = texture_->HitsPrimaryButton(position);
  const bool secondary_hovered = texture_->HitsSecondaryButton(position);

  texture_->SetPrimaryButtonHovered(primary_hovered);
  texture_->SetPrimaryButtonPressed(primary_hovered ? primary_down_ : false);
  texture_->SetSecondaryButtonHovered(secondary_hovered);
  texture_->SetSecondaryButtonPressed(secondary_hovered ? secondary_down_
                                                        : false);
}

UiTexture* Prompt::GetTexture() const {
  return texture_.get();
}

}  // namespace vr
