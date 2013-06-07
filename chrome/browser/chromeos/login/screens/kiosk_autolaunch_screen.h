// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/kiosk_autolaunch_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"

namespace chromeos {

// Representation independent class that controls screen showing auto launch
// warning to users.
class KioskAutolaunchScreen : public WizardScreen,
                              public KioskAutolaunchScreenActor::Delegate {
 public:
  KioskAutolaunchScreen(ScreenObserver* observer,
                        KioskAutolaunchScreenActor* actor);
  virtual ~KioskAutolaunchScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE {}
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE {}
  virtual std::string GetName() const OVERRIDE;

  // KioskAutolaunchScreenActor::Delegate implementation:
  virtual void OnExit(bool confirmed) OVERRIDE;
  virtual void OnActorDestroyed(KioskAutolaunchScreenActor* actor) OVERRIDE;

 private:
  KioskAutolaunchScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(KioskAutolaunchScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_AUTOLAUNCH_SCREEN_H_
