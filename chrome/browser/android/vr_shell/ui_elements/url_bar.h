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
#include "chrome/browser/android/vr_shell/ui_unsupported_mode.h"
#include "components/security_state/core/security_state.h"
#include "url/gurl.h"

namespace vr_shell {

class UrlBarTexture;

class UrlBar : public TexturedElement {
 public:
  UrlBar(int preferred_width,
         const base::Callback<void()>& back_button_callback,
         const base::Callback<void()>& security_icon_callback,
         const base::Callback<void(UiUnsupportedMode)>& failure_callback);
  ~UrlBar() override;

  void OnHoverEnter(const gfx::PointF& position) override;
  void OnHoverLeave() override;
  void OnMove(const gfx::PointF& position) override;
  void OnButtonDown(const gfx::PointF& position) override;
  void OnButtonUp(const gfx::PointF& position) override;
  bool HitTest(const gfx::PointF& point) const override;

  void OnBeginFrame(const base::TimeTicks& begin_frame_time) override;
  void SetEnabled(bool enabled) override;

  void SetHistoryButtonsEnabled(bool can_go_back);
  void SetURL(const GURL& gurl);
  void SetSecurityInfo(security_state::SecurityLevel level, bool malware);

 private:
  void UpdateTexture() override;
  UiTexture* GetTexture() const override;
  void OnStateUpdated(const gfx::PointF& position);

  std::unique_ptr<UrlBarTexture> texture_;
  base::Callback<void()> back_button_callback_;
  base::Callback<void()> security_icon_callback_;
  bool enabled_ = false;
  bool can_go_back_ = false;
  bool down_ = false;
  bool security_region_down_ = false;
  base::TimeTicks last_begin_frame_time_;
  base::TimeTicks last_update_time_;

  DISALLOW_COPY_AND_ASSIGN(UrlBar);
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_URL_BAR_H_
