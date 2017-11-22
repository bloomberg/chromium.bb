// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_H_
#define CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/model/color_scheme.h"

namespace vr {

// TODO (crbug/783788): remove AudioPermissionPromptTexture
class AudioPermissionPromptTexture;

class AudioPermissionPrompt : public TexturedElement {
 public:
  AudioPermissionPrompt(
      int preferred_width,
      const base::Callback<void()>& primary_button_callback,
      const base::Callback<void()>& secondary_buttton_callback);
  ~AudioPermissionPrompt() override;

  void SetTextureForTesting(AudioPermissionPromptTexture* texture);

  void OnHoverEnter(const gfx::PointF& position) override;
  void OnHoverLeave() override;
  void OnMove(const gfx::PointF& position) override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;

  void SetPrimaryButtonColors(const ButtonColors& colors);
  void SetSecondaryButtonColors(const ButtonColors& colors);
  void SetIconColor(SkColor color);

 private:
  UiTexture* GetTexture() const override;

  void OnStateUpdated(const gfx::PointF& position);

  bool primary_down_ = false;
  bool secondary_down_ = false;

  std::unique_ptr<AudioPermissionPromptTexture> texture_;

  base::Callback<void()> primary_button_callback_;
  base::Callback<void()> secondary_buttton_callback_;

  DISALLOW_COPY_AND_ASSIGN(AudioPermissionPrompt);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_H_
