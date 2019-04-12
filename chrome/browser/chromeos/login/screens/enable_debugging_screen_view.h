// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_VIEW_H_

#include <string>
#include "chrome/browser/chromeos/login/oobe_screen.h"

namespace chromeos {

class EnableDebuggingScreen;

// Interface between enable debugging screen and its representation.
// Note, do not forget to call OnViewDestroyed in the dtor.
class EnableDebuggingScreenView {
 public:
  constexpr static OobeScreen kScreenId =
      OobeScreen::SCREEN_OOBE_ENABLE_DEBUGGING;

  virtual ~EnableDebuggingScreenView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void SetDelegate(EnableDebuggingScreen* screen) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_VIEW_H_
