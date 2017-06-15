// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/android/vr_shell/textures/ui_texture.h"
#include "chrome/browser/android/vr_shell/ui_unsupported_mode.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace gfx {
class PointF;
class RenderText;
}  // namespace gfx

namespace vr_shell {

class RenderTextWrapper;
struct ColorScheme;

class UrlBarTexture : public UiTexture {
 public:
  enum DrawFlags {
    FLAG_BACK_HOVER = 1 << 0,
    FLAG_BACK_DOWN = 1 << 1,
  };

  UrlBarTexture(
      bool web_vr,
      const base::Callback<void(UiUnsupportedMode)>& failure_callback);
  ~UrlBarTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetURL(const GURL& gurl);
  void SetHistoryButtonsEnabled(bool can_go_back);
  void SetSecurityLevel(security_state::SecurityLevel level);

  bool HitsBackButton(const gfx::PointF& position) const;
  bool HitsUrlBar(const gfx::PointF& position) const;
  bool HitsSecurityIcon(const gfx::PointF& position) const;

  void SetBackButtonHovered(bool hovered);
  void SetBackButtonPressed(bool pressed);

  // Public for testability.
  static void ApplyUrlStyling(const base::string16& formatted_url,
                              const url::Parsed& parsed,
                              security_state::SecurityLevel security_level,
                              vr_shell::RenderTextWrapper* render_text,
                              const ColorScheme& color_scheme);

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;
  float ToPixels(float meters) const;
  bool HitsTransparentRegion(const gfx::PointF& meters, bool left) const;
  void RenderUrl(const gfx::Size& texture_size, const gfx::Rect& bounds);
  void OnSetMode() override;
  gfx::PointF SecurityIconPositionMeters() const;
  gfx::PointF UrlBarPositionMeters() const;
  SkColor GetLeftCornerColor() const;

  gfx::SizeF size_;
  bool back_hovered_ = false;
  bool back_pressed_ = false;
  bool can_go_back_ = false;

  GURL gurl_;
  security_state::SecurityLevel security_level_;

  std::unique_ptr<gfx::RenderText> url_render_text_;
  GURL last_drawn_gurl_;
  bool has_back_button_ = true;
  security_state::SecurityLevel last_drawn_security_level_;
  base::Callback<void(UiUnsupportedMode)> failure_callback_;

  DISALLOW_COPY_AND_ASSIGN(UrlBarTexture);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEXTURES_URL_BAR_TEXTURE_H_
