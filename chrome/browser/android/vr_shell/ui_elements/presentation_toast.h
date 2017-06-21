// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_PRESENTATION_TOAST_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_PRESENTATION_TOAST_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

namespace vr_shell {

class UiTexture;

// TODO(bshe): A lot code duplication with exit_warning_texture and
// insecure_content_transient_texture. Consider refactor(see crbug.com/735166).
class PresentationToast : public TexturedElement {
 public:
  explicit PresentationToast(int preferred_width);
  ~PresentationToast() override;

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<UiTexture> texture_;

  DISALLOW_COPY_AND_ASSIGN(PresentationToast);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_PRESENTATION_TOAST_H_
