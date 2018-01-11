// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_URL_BAR_H_
#define CHROME_BROWSER_VR_ELEMENTS_URL_BAR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace vr {

class UrlBarTexture;
struct ToolbarState;
struct UrlBarColors;

class UrlBar : public TexturedElement {
 public:
  UrlBar(
      int preferred_width,
      const base::RepeatingCallback<void()>& url_click_callback,
      const base::RepeatingCallback<void(UiUnsupportedMode)>& failure_callback);
  ~UrlBar() override;

  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;

  void SetToolbarState(const ToolbarState& state);
  void SetColors(const UrlBarColors& colors);

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<UrlBarTexture> texture_;
  base::RepeatingCallback<void()> url_click_callback_;
  base::RepeatingCallback<void(UiUnsupportedMode)> failure_callback_;

  bool security_region_down_ = false;
  bool url_down_ = false;

  DISALLOW_COPY_AND_ASSIGN(UrlBar);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_URL_BAR_H_
