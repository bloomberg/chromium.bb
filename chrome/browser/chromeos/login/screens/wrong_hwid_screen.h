// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/wrong_hwid_screen_actor.h"

namespace chromeos {

// Representation independent class that controls screen showing warning about
// malformed HWID to users.
class WrongHWIDScreen : public BaseScreen,
                        public WrongHWIDScreenActor::Delegate {
 public:
  WrongHWIDScreen(BaseScreenDelegate* base_screen_delegate,
                  WrongHWIDScreenActor* actor);
  virtual ~WrongHWIDScreen();

  // BaseScreen implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // WrongHWIDScreenActor::Delegate implementation:
  virtual void OnExit() override;
  virtual void OnActorDestroyed(WrongHWIDScreenActor* actor) override;

 private:
  WrongHWIDScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(WrongHWIDScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_WRONG_HWID_SCREEN_H_

