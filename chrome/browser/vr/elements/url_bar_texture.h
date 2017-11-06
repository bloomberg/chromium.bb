// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_URL_BAR_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_URL_BAR_TEXTURE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/toolbar_state.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "components/security_state/core/security_state.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

namespace gfx {
class PointF;
class RenderText;
}  // namespace gfx

namespace vr {

class RenderTextWrapper;
struct ColorScheme;

class UrlBarTexture : public UiTexture {
 public:
  enum DrawFlags {
    FLAG_BACK_HOVER = 1 << 0,
    FLAG_BACK_DOWN = 1 << 1,
  };

  UrlBarTexture(
      const base::Callback<void(UiUnsupportedMode)>& failure_callback);
  ~UrlBarTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetToolbarState(const ToolbarState& state);
  void SetHistoryButtonsEnabled(bool can_go_back);

  bool HitsBackButton(const gfx::PointF& position) const;
  bool HitsUrlBar(const gfx::PointF& position) const;
  bool HitsSecurityRegion(const gfx::PointF& position) const;

  void SetBackButtonHovered(bool hovered);
  void SetBackButtonPressed(bool pressed);

 protected:
  static void ApplyUrlStyling(const base::string16& formatted_url,
                              const url::Parsed& parsed,
                              security_state::SecurityLevel security_level,
                              RenderTextWrapper* render_text,
                              const ColorScheme& color_scheme);

  std::unique_ptr<gfx::RenderText> url_render_text_;

  // Rendered state for test purposes. The text rectangles represent regions
  // available to text, not the smaller area of the actual rendered text.
  base::string16 rendered_url_text_;
  gfx::Rect rendered_url_text_rect_;
  base::string16 rendered_security_text_;
  gfx::Rect rendered_security_text_rect_;

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;
  float ToPixels(float meters) const;
  float ToMeters(float pixels) const;
  bool HitsTransparentRegion(const gfx::PointF& meters, bool left) const;
  void RenderUrl(const gfx::Size& texture_size, const gfx::Rect& text_bounds);
  void OnSetMode() override;
  SkColor BackButtonColor() const;

  gfx::SizeF size_;
  bool back_hovered_ = false;
  bool back_pressed_ = false;
  bool can_go_back_ = false;

  ToolbarState state_;

  bool url_dirty_ = true;

  base::Callback<void(UiUnsupportedMode)> failure_callback_;
  gfx::RectF security_hit_region_ = gfx::RectF(0, 0, 0, 0);

  DISALLOW_COPY_AND_ASSIGN(UrlBarTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_URL_BAR_TEXTURE_H_
