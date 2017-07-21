// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TRANSIENCE_MANAGER_H_
#define CHROME_BROWSER_VR_ELEMENTS_TRANSIENCE_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/timer.h"
#include "chrome/browser/vr/elements/ui_element.h"

namespace vr {

class TransienceManager {
 public:
  TransienceManager(UiElement* element,
                    float opacity_when_enabled,
                    const base::TimeDelta& timeout);
  virtual ~TransienceManager() = default;

  void SetEnabled(bool enabled);
  void KickVisibilityIfEnabled();
  void EndVisibilityIfEnabled();

 private:
  void StartTimer();
  void OnTimeout();
  void Show();
  void Hide();

  UiElement* element_;
  float opacity_when_enabled_;
  base::TimeDelta timeout_;
  bool enabled_ = false;
  base::OneShotTimer visibility_timer_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TRANSIENCE_MANAGER_H_
