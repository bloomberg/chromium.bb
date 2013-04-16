// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/eula_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockEulaScreen : public EulaScreen {
 public:
  MockEulaScreen(ScreenObserver* screen_observer, EulaScreenActor* actor);
  virtual ~MockEulaScreen();
};

class MockEulaScreenActor : public EulaScreenActor {
 public:
  MockEulaScreenActor();
  virtual ~MockEulaScreenActor();

  virtual void SetDelegate(Delegate* delegate);

  MOCK_METHOD0(PrepareToShow, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());

  MOCK_CONST_METHOD0(GetName, std::string());

  MOCK_METHOD1(MockSetDelegate, void(Delegate* delegate));
  MOCK_METHOD1(OnPasswordFetched, void(const std::string& tpm_password));

 private:
  Delegate* delegate_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_EULA_SCREEN_H_
