// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_LOCATION_ACCESS_INDICATOR_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_LOCATION_ACCESS_INDICATOR_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

namespace vr_shell {

class UiTexture;

class LocationAccessIndicator : public TexturedElement {
 public:
  explicit LocationAccessIndicator(int preferred_width);
  ~LocationAccessIndicator() override;

 private:
  UiTexture* GetTexture() const override;
  std::unique_ptr<UiTexture> texture_;

  DISALLOW_COPY_AND_ASSIGN(LocationAccessIndicator);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_LOCATION_ACCESS_INDICATOR_H_
