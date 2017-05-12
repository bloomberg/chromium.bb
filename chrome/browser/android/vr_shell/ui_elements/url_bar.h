// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_URL_BAR_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_URL_BAR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/android/vr_shell/ui_elements/textured_element.h"
#include "url/gurl.h"

namespace vr_shell {

class UrlBarTexture;

class UrlBar : public TexturedElement {
 public:
  explicit UrlBar(int preferred_width);
  ~UrlBar() override;

  void OnHoverEnter(gfx::PointF position) override;
  void OnHoverLeave() override;
  void OnBeginFrame(const base::TimeTicks& begin_frame_time) override;
  void OnButtonUp(gfx::PointF position) override;
  void SetEnabled(bool enabled);

  void SetURL(const GURL& gurl);
  void SetSecurityLevel(int level);
  void SetBackButtonCallback(const base::Callback<void()>& callback);

 private:
  void UpdateTexture() override;
  UiTexture* GetTexture() const override;
  std::unique_ptr<UrlBarTexture> texture_;
  base::Callback<void()> back_button_callback_;
  bool enabled_ = false;
  base::TimeTicks last_begin_frame_time_;
  base::TimeTicks last_update_time_;

  DISALLOW_COPY_AND_ASSIGN(UrlBar);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_URL_BAR_H_
