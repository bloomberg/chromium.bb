// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_WEBVR_URL_TOAST_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_WEBVR_URL_TOAST_TEXTURE_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/toolbar_state.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"

namespace gfx {
class RenderText;
}  // namespace gfx

namespace vr {

class WebVrUrlToastTexture : public UiTexture {
 public:
  WebVrUrlToastTexture(
      const base::Callback<void(UiUnsupportedMode)>& failure_callback);
  ~WebVrUrlToastTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetToolbarState(const ToolbarState& state);

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;
  void RenderUrl(const gfx::Size& texture_size, const gfx::Rect& text_bounds);
  float ToPixels(float meters) const;

  gfx::SizeF size_;
  ToolbarState state_;

  bool url_dirty_ = true;
  std::unique_ptr<gfx::RenderText> url_render_text_;

  base::Callback<void(UiUnsupportedMode)> failure_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebVrUrlToastTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_WEBVR_URL_TOAST_TEXTURE_H_
