// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_URL_BAR_TEXTURE_H_
#define CHROME_BROWSER_VR_ELEMENTS_URL_BAR_TEXTURE_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/vr/elements/omnibox_formatting.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/model/color_scheme.h"
#include "chrome/browser/vr/model/toolbar_state.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "components/security_state/core/security_state.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

namespace vr {

class RenderTextWrapper;

class UrlBarTexture : public UiTexture {
 public:
  UrlBarTexture(
      const base::Callback<void(UiUnsupportedMode)>& failure_callback);
  ~UrlBarTexture() override;
  gfx::Size GetPreferredTextureSize(int width) const override;
  gfx::SizeF GetDrawnSize() const override;

  void SetColors(const UrlTextColors& colors);
  void SetToolbarState(const ToolbarState& state);

 protected:
  static void ApplyUrlStyling(const base::string16& formatted_url,
                              const url::Parsed& parsed,
                              RenderTextWrapper* render_text,
                              const UrlTextColors& colors);

 private:
  void Draw(SkCanvas* canvas, const gfx::Size& texture_size) override;
  float ToPixels(float meters) const;
  float ToMeters(float pixels) const;

  gfx::SizeF size_;
  ToolbarState state_;
  UrlTextColors colors_;

  base::Callback<void(UiUnsupportedMode)> failure_callback_;

  DISALLOW_COPY_AND_ASSIGN(UrlBarTexture);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_URL_BAR_TEXTURE_H_
