// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_PROMPT_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_PROMPT_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

class PromptTexture : public UiTexture {
 public:
  PromptTexture(int content_message_id,
                const gfx::VectorIcon& icon,
                int primary_button_message_id,
                int secondary_button_message_id);
  ~PromptTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const;

  void SetPrimaryButtonHovered(bool hovered);
  void SetPrimaryButtonPressed(bool pressed);
  void SetSecondaryButtonHovered(bool hovered);
  void SetSecondaryButtonPressed(bool pressed);
  virtual void SetContentMessageId(int message_id);

  virtual bool HitsSecondaryButton(const gfx::PointF& position) const;
  virtual bool HitsPrimaryButton(const gfx::PointF& position) const;

  void SetPrimaryButtonColors(const ButtonColors& colors);
  void SetSecondaryButtonColors(const ButtonColors& colors);
  void SetIconColor(SkColor color);

 private:
  float ToPixels(float meters) const;
  gfx::PointF PercentToPixels(const gfx::PointF& percent) const;
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

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
  const gfx::VectorIcon& icon_;

  int primary_button_message_id_ = -1;
  int secondary_button_message_id_ = -1;
  int content_message_id_ = -1;

  DISALLOW_COPY_AND_ASSIGN(PromptTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_PROMPT_TEXTURE_H_
