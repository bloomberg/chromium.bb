// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TRANSIENCE_MANAGER_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TRANSIENCE_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/vr_shell/ui_elements/ui_element.h"

namespace vr_shell {

class TransienceManager {
 public:
  TransienceManager(UiElement* element, const base::TimeDelta& timeout);
  virtual ~TransienceManager() = default;

  void SetEnabled(bool enabled);
  void KickVisibilityIfEnabled();
  void EndVisibilityIfEnabled();

 private:
  void StartTimer();
  void OnTimeout();

  UiElement* element_;
  base::TimeDelta timeout_;
  bool enabled_ = false;
  base::OneShotTimer visibility_timer_;
};

}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_UI_ELEMENTS_TRANSIENCE_MANAGER_H_
