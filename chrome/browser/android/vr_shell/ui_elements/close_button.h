// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_CLOSE_BUTTON_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_CLOSE_BUTTON_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

namespace vr_shell {

class CloseButtonTexture;
class UiTexture;

class CloseButton : public TexturedElement {
 public:
  explicit CloseButton(base::Callback<void()> click_handler);
  ~CloseButton() override;

  void OnHoverLeave() override;
  void OnHoverEnter(gfx::PointF position) override;
  void OnButtonDown(gfx::PointF position) override;
  void OnButtonUp(gfx::PointF position) override;

 private:
  UiTexture* GetTexture() const override;
  void OnStateUpdated();

  std::unique_ptr<CloseButtonTexture> texture_;
  bool down_ = false;
  bool hover_ = false;
  base::Callback<void()> click_handler_;

  DISALLOW_COPY_AND_ASSIGN(CloseButton);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_CLOSE_BUTTON_H_
