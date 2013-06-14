// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"

namespace chromeos {

// Representation independent class that controls screen for enabling
// consumer kiosk mode.
class KioskEnableScreen : public WizardScreen,
                          public KioskEnableScreenActor::Delegate {
 public:
  KioskEnableScreen(ScreenObserver* observer, KioskEnableScreenActor* actor);
  virtual ~KioskEnableScreen();

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE {}
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE {}
  virtual std::string GetName() const OVERRIDE;

  // KioskEnableScreenActor::Delegate implementation:
  virtual void OnExit() OVERRIDE;
  virtual void OnActorDestroyed(KioskEnableScreenActor* actor) OVERRIDE;

 private:
  KioskEnableScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(KioskEnableScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
