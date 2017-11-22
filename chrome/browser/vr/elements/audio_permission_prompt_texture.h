// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

class AudioPermissionPromptTexture : public UiTexture {
 public:
  AudioPermissionPromptTexture();
  ~AudioPermissionPromptTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetPrimaryButtonHovered(bool hovered);
  void SetPrimaryButtonPressed(bool pressed);
  void SetSecondaryButtonHovered(bool hovered);
  void SetSecondaryButtonPressed(bool pressed);

  virtual bool HitsSecondaryButton(const gfx::PointF& position) const;
  virtual bool HitsPrimaryButton(const gfx::PointF& position) const;

  void SetPrimaryButtonColors(const ButtonColors& colors);
  void SetSecondaryButtonColors(const ButtonColors& colors);
  void SetIconColor(SkColor color);

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  float ToPixels(float meters) const;
  gfx::PointF PercentToPixels(const gfx::PointF& percent) const;

  gfx::RectF secondary_button_rect_;
  gfx::RectF primary_button_rect_;
  gfx::SizeF size_;

  bool primary_hovered_ = false;
  bool primary_pressed_ = false;
  bool secondary_hovered_ = false;
  bool secondary_pressed_ = false;

  ButtonColors primary_button_colors_;
  ButtonColors secondary_button_colors_;
  SkColor icon_color_ = SK_ColorBLACK;

  DISALLOW_COPY_AND_ASSIGN(AudioPermissionPromptTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_AUDIO_PERMISSION_PROMPT_TEXTURE_H_
