// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/reset_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"

namespace chromeos {

// Representation independent class that controls screen showing reset to users.
class ResetScreen : public WizardScreen,
                    public ResetScreenActor::Delegate {
 public:
  ResetScreen(ScreenObserver* observer, ResetScreenActor* actor);
  virtual ~ResetScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // ResetScreenActor::Delegate implementation:
  virtual void OnExit() OVERRIDE;
  virtual void OnActorDestroyed(ResetScreenActor* actor) OVERRIDE;

 private:
  ResetScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(ResetScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RESET_SCREEN_H_
