// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_NETWORK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_NETWORK_SCREEN_H_
#pragma once

#include "chrome/browser/chromeos/login/network_screen.h"
#include "chrome/browser/chromeos/login/network_screen_actor.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockNetworkScreen : public NetworkScreen {
 public:
  explicit MockNetworkScreen(ScreenObserver* observer);
  virtual ~MockNetworkScreen();
};

class MockNetworkScreenActor : public NetworkScreenActor {
 public:
  MockNetworkScreenActor();
  virtual ~MockNetworkScreenActor();

  MOCK_METHOD1(SetDelegate, void(Delegate* screen));
  MOCK_METHOD0(PrepareToShow, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD1(ShowError, void(const string16& message));
  MOCK_METHOD0(ClearErrors, void());
  MOCK_METHOD2(ShowConnectingStatus,
               void(bool connecting, const string16& network_id));
  MOCK_METHOD1(EnableContinue, void(bool enabled));
  MOCK_CONST_METHOD0(IsContinueEnabled, bool());
  MOCK_CONST_METHOD0(IsConnecting, bool());
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_NETWORK_SCREEN_H_
