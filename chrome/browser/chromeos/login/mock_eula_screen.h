// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_EULA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_EULA_SCREEN_H_
#pragma once

#include "chrome/browser/chromeos/login/eula_screen.h"
#include "chrome/browser/chromeos/login/eula_screen_actor.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockEulaScreen : public EulaScreen {
 public:
  explicit MockEulaScreen(ScreenObserver* screen_observer);
  virtual ~MockEulaScreen();
};

class MockEulaScreenActor : public EulaScreenActor {
 public:
  MockEulaScreenActor();
  virtual ~MockEulaScreenActor();

  MOCK_METHOD0(PrepareToShow, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_CONST_METHOD0(IsUsageStatsChecked, bool());
  MOCK_METHOD1(SetDelegate, void(Delegate* delegate));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_EULA_SCREEN_H_
