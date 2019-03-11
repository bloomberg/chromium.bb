// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen_view.h"

namespace chromeos {

// Representation independent class that controls screen showing warning about
// malformed HWID to users.
class WrongHWIDScreen : public BaseScreen,
                        public WrongHWIDScreenView::Delegate {
 public:
  WrongHWIDScreen(WrongHWIDScreenView* view,
                  const base::RepeatingClosure& exit_callback);
  ~WrongHWIDScreen() override;

  // BaseScreen implementation:
  void Show() override;
  void Hide() override;

  // WrongHWIDScreenView::Delegate implementation:
  void OnExit() override;
  void OnViewDestroyed(WrongHWIDScreenView* view) override;

 private:
  WrongHWIDScreenView* view_;
  base::RepeatingClosure exit_callback_;

  DISALLOW_COPY_AND_ASSIGN(WrongHWIDScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_
