// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_H_

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/textures/exclusive_screen_toast_texture.h"
#include "chrome/browser/android/vr_shell/ui_elements/simple_textured_element.h"

namespace vr_shell {

class ExclusiveScreenToast
    : public SimpleTexturedElement<ExclusiveScreenToastTexture> {
 public:
  explicit ExclusiveScreenToast(int preferred_width);
  ~ExclusiveScreenToast() override;

 private:
  void UpdateElementSize() override;

  DISALLOW_COPY_AND_ASSIGN(ExclusiveScreenToast);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_EXCLUSIVE_SCREEN_TOAST_H_
