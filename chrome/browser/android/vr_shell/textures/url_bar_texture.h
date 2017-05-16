// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "url/gurl.h"

namespace gfx {
class PointF;
class RenderText;
}  // namespace gfx

namespace vr_shell {

class UrlBarTexture : public UiTexture {
 public:
  enum DrawFlags {
    FLAG_BACK_HOVER = 1 << 0,
    FLAG_BACK_DOWN = 1 << 1,
  };

  UrlBarTexture();
  ~UrlBarTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;
  bool SetDrawFlags(int draw_flags) override;

  void SetURL(const GURL& gurl);
  void SetSecurityLevel(int level);

  bool HitsBackButton(const gfx::PointF& position) const;
  bool HitsUrlBar(const gfx::PointF& position) const;

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;
  float ToPixels(float meters) const;
  bool HitsTransparentRegion(const gfx::PointF& meters, bool left) const;

  gfx::SizeF size_;
  int security_level_;
  GURL gurl_;
  GURL last_drawn_gurl_;
  std::vector<std::unique_ptr<gfx::RenderText>> gurl_render_texts_;

  DISALLOW_COPY_AND_ASSIGN(UrlBarTexture);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_
