// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/exit_prompt.h"

#include "chrome/browser/vr/elements/exit_prompt_texture.h"

namespace vr {

ExitPrompt::ExitPrompt(int preferred_width,
                       const ExitPromptCallback& result_callback)
    : TexturedElement(preferred_width),
      texture_(std::make_unique<ExitPromptTexture>()),
      result_callback_(result_callback) {}

ExitPrompt::ExitPrompt(int preferred_width,
                       const ExitPromptCallback& result_callback,
                       std::unique_ptr<ExitPromptTexture> texture)
    : TexturedElement(preferred_width),
      texture_(std::move(texture)),
      result_callback_(result_callback) {}

ExitPrompt::~ExitPrompt() = default;

void ExitPrompt::SetContentMessageId(int message_id) {
  texture_->SetContentMessageId(message_id);
}

void ExitPrompt::SetTextureForTesting(
    std::unique_ptr<ExitPromptTexture> texture) {
  texture_ = std::move(texture);
}

void ExitPrompt::OnHoverEnter(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void ExitPrompt::OnHoverLeave() {
  OnStateUpdated(gfx::PointF(std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()));
}

void ExitPrompt::OnMove(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void ExitPrompt::OnButtonDown(const gfx::PointF& position) {
  if (texture_->HitsPrimaryButton(position))
    primary_down_ = true;
  else if (texture_->HitsSecondaryButton(position))
    secondary_down_ = true;
  OnStateUpdated(position);
}

void ExitPrompt::OnButtonUp(const gfx::PointF& position) {
  if (primary_down_ && texture_->HitsPrimaryButton(position))
    result_callback_.Run(PRIMARY, reason_);
  else if (secondary_down_ && texture_->HitsSecondaryButton(position))
    result_callback_.Run(SECONDARY, reason_);

  primary_down_ = false;
  secondary_down_ = false;

  OnStateUpdated(position);
}

void ExitPrompt::SetPrimaryButtonColors(const ButtonColors& colors) {
  texture_->SetPrimaryButtonColors(colors);
}

void ExitPrompt::SetSecondaryButtonColors(const ButtonColors& colors) {
  texture_->SetSecondaryButtonColors(colors);
}

void ExitPrompt::ClickPrimaryButtonForTesting() {
  result_callback_.Run(PRIMARY, reason_);
}

void ExitPrompt::ClickSecondaryButtonForTesting() {
  result_callback_.Run(SECONDARY, reason_);
}

void ExitPrompt::Cancel() {
  result_callback_.Run(NONE, reason_);
}

void ExitPrompt::OnStateUpdated(const gfx::PointF& position) {
  const bool primary_hovered = texture_->HitsPrimaryButton(position);
  const bool secondary_hovered = texture_->HitsSecondaryButton(position);

  texture_->SetPrimaryButtonHovered(primary_hovered);
  texture_->SetPrimaryButtonPressed(primary_hovered ? primary_down_ : false);
  texture_->SetSecondaryButtonHovered(secondary_hovered);
  texture_->SetSecondaryButtonPressed(secondary_hovered ? secondary_down_
                                                        : false);
}

UiTexture* ExitPrompt::GetTexture() const {
  return texture_.get();
}

}  // namespace vr
