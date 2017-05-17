// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_BUTTON_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_BUTTON_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

namespace gfx {
class PointF;
}  // gfx

namespace vr_shell {

class ButtonTexture;

class Button : public TexturedElement {
 public:
  explicit Button(base::Callback<void()> click_handler,
                  std::unique_ptr<ButtonTexture> texture);
  ~Button() override;

  void OnHoverLeave() override;
  void OnHoverEnter(const gfx::PointF& position) override;
  void OnMove(const gfx::PointF& position) override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;
  bool HitTest(const gfx::PointF& point) const override;

 private:
  UiTexture* GetTexture() const override;
  void OnStateUpdated(const gfx::PointF& position);

  std::unique_ptr<ButtonTexture> texture_;
  bool down_ = false;
  base::Callback<void()> click_handler_;

  DISALLOW_COPY_AND_ASSIGN(Button);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_BUTTON_H_
