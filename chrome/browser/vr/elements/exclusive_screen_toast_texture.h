// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_texture.h"

namespace vr {

class ExclusiveScreenToastTexture : public UiTexture {
 public:
  ExclusiveScreenToastTexture();
  ~ExclusiveScreenToastTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveScreenToastTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_TEXTURE_H_
