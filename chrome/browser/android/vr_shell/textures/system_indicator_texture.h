// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_SYSTEM_INDICATOR_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_SYSTEM_INDICATOR_TEXTURE_H_

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "ui/gfx/vector_icon_types.h"

namespace vr_shell {

class SystemIndicatorTexture : public UiTexture {
 public:
  SystemIndicatorTexture(const gfx::VectorIcon& icon, int message_id);
  explicit SystemIndicatorTexture(const gfx::VectorIcon& icon);
  ~SystemIndicatorTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;

  gfx::SizeF size_;
  const gfx::VectorIcon& icon_;
  int message_id_;
  bool has_text_;

  DISALLOW_COPY_AND_ASSIGN(SystemIndicatorTexture);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_SYSTEM_INDICATOR_TEXTURE_H_
