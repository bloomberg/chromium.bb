// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_LOADING_INDICATOR_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_LOADING_INDICATOR_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"

namespace vr_shell {

class LoadingIndicatorTexture;

class LoadingIndicator : public TexturedElement {
 public:
  explicit LoadingIndicator(int preferred_width);
  ~LoadingIndicator() override;

  void SetLoading(bool loading);
  void SetLoadProgress(float progress);

  void SetEnabled(bool enabled) override;
  void OnBeginFrame(const base::TimeTicks& ticks) override;

 private:
  UiTexture* GetTexture() const override;
  std::unique_ptr<LoadingIndicatorTexture> texture_;

  void ResetVisibilityTimer();
  void SetVisibility();

  base::OneShotTimer visibility_timer_;
  bool enabled_ = false;
  bool loading_ = false;

  DISALLOW_COPY_AND_ASSIGN(LoadingIndicator);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_LOADING_INDICATOR_H_
