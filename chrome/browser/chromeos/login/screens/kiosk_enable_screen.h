// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/kiosk_enable_screen_actor.h"

namespace chromeos {

// Representation independent class that controls screen for enabling
// consumer kiosk mode.
class KioskEnableScreen : public BaseScreen,
                          public KioskEnableScreenActor::Delegate {
 public:
  KioskEnableScreen(BaseScreenDelegate* base_screen_delegate,
                    KioskEnableScreenActor* actor);
  virtual ~KioskEnableScreen();

  // BaseScreen implementation:
  virtual void PrepareToShow() override {}
  virtual void Show() override;
  virtual void Hide() override {}
  virtual std::string GetName() const override;

  // KioskEnableScreenActor::Delegate implementation:
  virtual void OnExit() override;
  virtual void OnActorDestroyed(KioskEnableScreenActor* actor) override;

 private:
  KioskEnableScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(KioskEnableScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_KIOSK_ENABLE_SCREEN_H_
