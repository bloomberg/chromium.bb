// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/network_screen.h"
#include "chrome/browser/chromeos/login/screens/network_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockNetworkScreen : public NetworkScreen {
 public:
  MockNetworkScreen(ScreenObserver* observer, NetworkScreenActor* actor);
  virtual ~MockNetworkScreen();
};

class MockNetworkScreenActor : public NetworkScreenActor {
 public:
  MockNetworkScreenActor();
  virtual ~MockNetworkScreenActor();

  virtual void SetDelegate(Delegate* delegate);

  MOCK_METHOD1(MockSetDelegate, void(Delegate* delegate));
  MOCK_METHOD0(PrepareToShow, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(ShowError, void(const base::string16& message));
  MOCK_METHOD0(ClearErrors, void());
  MOCK_METHOD2(ShowConnectingStatus,
               void(bool connecting, const base::string16& network_id));
  MOCK_METHOD1(EnableContinue, void(bool enabled));
  MOCK_CONST_METHOD0(IsContinueEnabled, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());

  private:
   Delegate* delegate_;

};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_NETWORK_SCREEN_H_
