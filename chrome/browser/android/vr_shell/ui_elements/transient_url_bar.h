// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TRANSIENT_URL_BAR_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TRANSIENT_URL_BAR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"
#include "chrome/browser/android/vr_shell/ui_unsupported_mode.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace vr_shell {

class UrlBarTexture;

// The non-interactive URL bar that shows for some time when WebVR content is
// autopresented.
class TransientUrlBar : public TexturedElement {
 public:
  TransientUrlBar(
      int preferred_width,
      const base::Callback<void(UiUnsupportedMode)>& failure_callback);
  ~TransientUrlBar() override;

  void SetURL(const GURL& gurl);
  void SetSecurityInfo(security_state::SecurityLevel level, bool malware);

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<UrlBarTexture> texture_;

  DISALLOW_COPY_AND_ASSIGN(TransientUrlBar);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TRANSIENT_URL_BAR_H_
