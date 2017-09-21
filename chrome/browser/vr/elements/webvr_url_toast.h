// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_WEBVR_URL_TOAST_H_
#define CHROME_BROWSER_VR_ELEMENTS_WEBVR_URL_TOAST_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/vr/elements/textured_element.h"
#include "chrome/browser/vr/elements/transient_element.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace vr {

class WebVrUrlToastTexture;
struct ToolbarState;

// The non-interactive URL bar that shows for some time when WebVR content is
// autopresented.
class WebVrUrlToast : public TexturedElement {
 public:
  WebVrUrlToast(
      int preferred_width,
      const base::Callback<void(UiUnsupportedMode)>& failure_callback);
  ~WebVrUrlToast() override;

  void SetToolbarState(const ToolbarState& state);

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<WebVrUrlToastTexture> texture_;

  DISALLOW_COPY_AND_ASSIGN(WebVrUrlToast);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_WEBVR_URL_TOAST_H_
