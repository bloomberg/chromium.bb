// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/audio_permission_prompt.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/vr/elements/audio_permission_prompt_texture.h"

namespace vr {

AudioPermissionPrompt::AudioPermissionPrompt(
    int preferred_width,
    const base::Callback<void()>& primary_button_callback,
    const base::Callback<void()>& secondary_buttton_callback)
    : TexturedElement(preferred_width),
      texture_(base::MakeUnique<AudioPermissionPromptTexture>()),
      primary_button_callback_(primary_button_callback),
      secondary_buttton_callback_(secondary_buttton_callback) {}

AudioPermissionPrompt::~AudioPermissionPrompt() = default;

void AudioPermissionPrompt::SetTextureForTesting(
    AudioPermissionPromptTexture* texture) {
  texture_.reset(texture);
}

void AudioPermissionPrompt::OnHoverEnter(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void AudioPermissionPrompt::OnHoverLeave() {
  OnStateUpdated(gfx::PointF(std::numeric_limits<float>::max(),
                             std::numeric_limits<float>::max()));
}

void AudioPermissionPrompt::OnMove(const gfx::PointF& position) {
  OnStateUpdated(position);
}

void AudioPermissionPrompt::OnButtonDown(const gfx::PointF& position) {
  if (texture_->HitsPrimaryButton(position))
    primary_down_ = true;
  else if (texture_->HitsSecondaryButton(position))
    secondary_down_ = true;
  OnStateUpdated(position);
}

void AudioPermissionPrompt::OnButtonUp(const gfx::PointF& position) {
  if (primary_down_ && texture_->HitsPrimaryButton(position))
    primary_button_callback_.Run();
  else if (secondary_down_ && texture_->HitsSecondaryButton(position))
    secondary_buttton_callback_.Run();

  primary_down_ = false;
  secondary_down_ = false;

  OnStateUpdated(position);
}

void AudioPermissionPrompt::SetPrimaryButtonColors(const ButtonColors& colors) {
  texture_->SetPrimaryButtonColors(colors);
}

void AudioPermissionPrompt::SetSecondaryButtonColors(
    const ButtonColors& colors) {
  texture_->SetSecondaryButtonColors(colors);
}

void AudioPermissionPrompt::SetIconColor(SkColor color) {
  texture_->SetIconColor(color);
}

void AudioPermissionPrompt::OnStateUpdated(const gfx::PointF& position) {
  const bool primary_hovered = texture_->HitsPrimaryButton(position);
  const bool secondary_hovered = texture_->HitsSecondaryButton(position);

  texture_->SetPrimaryButtonHovered(primary_hovered);
  texture_->SetPrimaryButtonPressed(primary_hovered ? primary_down_ : false);
  texture_->SetSecondaryButtonHovered(secondary_hovered);
  texture_->SetSecondaryButtonPressed(secondary_hovered ? secondary_down_
                                                        : false);
}

UiTexture* AudioPermissionPrompt::GetTexture() const {
  return texture_.get();
}

}  // namespace vr
