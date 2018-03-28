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
struct UrlTextColors;

class UrlBar : public TexturedElement {
 public:
  UrlBar(
      int preferred_width,
      const base::RepeatingCallback<void(UiUnsupportedMode)>& failure_callback);
  ~UrlBar() override;

  void SetToolbarState(const ToolbarState& state);
  void SetColors(const UrlTextColors& colors);
  void SetSingleColor(SkColor color);

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<UrlBarTexture> texture_;
  base::RepeatingCallback<void(UiUnsupportedMode)> failure_callback_;

  DISALLOW_COPY_AND_ASSIGN(UrlBar);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_URL_BAR_H_
