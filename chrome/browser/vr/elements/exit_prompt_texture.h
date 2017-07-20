// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/geometry/rect_f.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

class ExitPromptTexture : public UiTexture {
 public:
  ExitPromptTexture();
  ~ExitPromptTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetPrimaryButtonHovered(bool hovered);
  void SetPrimaryButtonPressed(bool pressed);
  void SetSecondaryButtonHovered(bool hovered);
  void SetSecondaryButtonPressed(bool pressed);
  void SetContentMessageId(int message_id);

  virtual bool HitsSecondaryButton(const gfx::PointF& position) const;
  virtual bool HitsPrimaryButton(const gfx::PointF& position) const;

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  SkColor GetPrimaryButtonColor() const;
  SkColor GetSecondaryButtonColor() const;
  float ToPixels(float meters) const;
  gfx::PointF PercentToPixels(const gfx::PointF& percent) const;

  gfx::RectF secondary_button_rect_;
  gfx::RectF primary_button_rect_;
  gfx::SizeF size_;

  bool primary_hovered_ = false;
  bool primary_pressed_ = false;
  bool secondary_hovered_ = false;
  bool secondary_pressed_ = false;
  int content_message_id_ = -1;

  DISALLOW_COPY_AND_ASSIGN(ExitPromptTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_EXIT_PROMPT_TEXTURE_H_
