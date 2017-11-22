// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_SYSTEM_INDICATOR_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_SYSTEM_INDICATOR_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr {

// TODO(asimjour): compose the system indicator out of primitives.
class SystemIndicatorTexture : public UiTexture {
 public:
  SystemIndicatorTexture();
  ~SystemIndicatorTexture() override;

  void SetIcon(const gfx::VectorIcon& icon);
  void SetMessageId(int id);

 private:
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
  gfx::VectorIcon icon_;
  int message_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SystemIndicatorTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_SYSTEM_INDICATOR_TEXTURE_H_
