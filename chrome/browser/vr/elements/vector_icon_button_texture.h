// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_BUTTON_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_BUTTON_TEXTURE_H_

#include "chrome/browser/vr/elements/button_texture.h"
#include "ui/gfx/vector_icon_types.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace vr {

class VectorIconButtonTexture : public ButtonTexture {
 public:
  explicit VectorIconButtonTexture(const gfx::VectorIcon& icon);
  ~VectorIconButtonTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;
  bool HitTest(const gfx::PointF& point) const override;

 private:
  void Draw(SkCanvas* sk_canvas, const gfx::Size& texture_size) override;

  gfx::VectorIcon icon_no_1x_{nullptr, nullptr};
  gfx::SizeF size_;

  DISALLOW_COPY_AND_ASSIGN(VectorIconButtonTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_VECTOR_ICON_BUTTON_TEXTURE_H_
