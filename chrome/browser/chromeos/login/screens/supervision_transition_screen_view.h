// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_TRANSITION_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_TRANSITION_SCREEN_VIEW_H_

#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

class SupervisionTransitionScreen;

// Interface for dependency injection between SupervisionTransitionScreen
// and its WebUI representation.
class SupervisionTransitionScreenView {
 public:
  constexpr static OobeScreen kScreenId =
      OobeScreen::SCREEN_SUPERVISION_TRANSITION;

  virtual ~SupervisionTransitionScreenView() {}

  virtual void Bind(SupervisionTransitionScreen* screen) = 0;
  virtual void Unbind() = 0;
  virtual void Show() = 0;
  virtual void Hide() = 0;

 protected:
  SupervisionTransitionScreenView() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SupervisionTransitionScreenView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_SUPERVISION_TRANSITION_SCREEN_VIEW_H_
