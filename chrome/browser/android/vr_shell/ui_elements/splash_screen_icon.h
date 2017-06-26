// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_SPLASH_SCREEN_ICON_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_SPLASH_SCREEN_ICON_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

class SkBitmap;

namespace vr_shell {

class SplashScreenIconTexture;

class SplashScreenIcon : public TexturedElement {
 public:
  explicit SplashScreenIcon(int preferred_width);
  ~SplashScreenIcon() override;

  void SetSplashScreenIconBitmap(const SkBitmap& bitmap);

 private:
  UiTexture* GetTexture() const override;

  std::unique_ptr<SplashScreenIconTexture> texture_;

  DISALLOW_COPY_AND_ASSIGN(SplashScreenIcon);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_SPLASH_SCREEN_ICON_H_
