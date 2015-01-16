// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/enable_debugging_screen_actor.h"

namespace chromeos {

// Representation independent class that controls screen showing enable
// debugging screen to users.
class EnableDebuggingScreen : public BaseScreen,
                              public EnableDebuggingScreenActor::Delegate {
 public:
  EnableDebuggingScreen(BaseScreenDelegate* delegate,
                        EnableDebuggingScreenActor* actor);
  ~EnableDebuggingScreen() override;

  // BaseScreen implementation:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  std::string GetName() const override;

  // EnableDebuggingScreenActor::Delegate implementation:
  void OnExit(bool success) override;
  void OnActorDestroyed(EnableDebuggingScreenActor* actor) override;

 private:
  EnableDebuggingScreenActor* actor_;

  DISALLOW_COPY_AND_ASSIGN(EnableDebuggingScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_ENABLE_DEBUGGING_SCREEN_H_
