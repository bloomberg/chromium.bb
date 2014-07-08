// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ERROR_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ERROR_SCREEN_H_

#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockErrorScreen : public ErrorScreen {
 public:
  MockErrorScreen(ScreenObserver* screen_observer, ErrorScreenActor* actor);
  virtual ~MockErrorScreen();
};

class MockErrorScreenActor : public ErrorScreenActor {
 public:
  MockErrorScreenActor();
  virtual ~MockErrorScreenActor();

  MOCK_METHOD1(SetDelegate, void(ErrorScreenActorDelegate* delegate));
  MOCK_METHOD2(Show, void(OobeDisplay::Screen parent_screen,
                          base::DictionaryValue* params));
  MOCK_METHOD3(Show, void(OobeDisplay::Screen parent_screen,
                          base::DictionaryValue* params,
                          const base::Closure& on_hide));
  MOCK_METHOD0(Hide, void(void));
  MOCK_METHOD0(FixCaptivePortal, void(void));
  MOCK_METHOD0(ShowCaptivePortal, void(void));
  MOCK_METHOD0(HideCaptivePortal, void(void));
  MOCK_METHOD1(SetUIState, void(ErrorScreen::UIState ui_state));
  MOCK_METHOD2(SetErrorState, void(ErrorScreen::ErrorState error_state,
                                   const std::string& network));
  MOCK_METHOD1(AllowGuestSignin, void(bool allowed));
  MOCK_METHOD1(AllowOfflineLogin, void(bool allowed));
  MOCK_METHOD1(ShowConnectingIndicator, void(bool show));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_MOCK_ERROR_SCREEN_H_
