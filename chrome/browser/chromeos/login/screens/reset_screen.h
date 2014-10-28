// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/reset_screen_actor.h"

namespace chromeos {

// Representation independent class that controls screen showing reset to users.
class ResetScreen : public BaseScreen, public ResetScreenActor::Delegate {
 public:
  ResetScreen(BaseScreenDelegate* base_screen_delegate,
              ResetScreenActor* actor);
  virtual ~ResetScreen();

  // BaseScreen implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // ResetScreenActor::Delegate implementation:
  virtual void OnExit() override;
  virtual void OnActorDestroyed(ResetScreenActor* actor) override;

 private:
  ResetScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(ResetScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_
