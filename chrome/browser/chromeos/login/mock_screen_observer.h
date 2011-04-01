// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_SCREEN_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_SCREEN_OBSERVER_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/screen_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

// Interface that handles notifications received from any of login wizard
// screens.
class MockScreenObserver : public ScreenObserver {
 public:
  MockScreenObserver() {}
  virtual ~MockScreenObserver() {}
  MOCK_METHOD1(OnExit, void(ExitCodes));
  MOCK_METHOD2(OnSetUserNamePassword,
               void(const std::string&, const std::string&));
  MOCK_METHOD1(set_usage_statistics_reporting, void(bool));
  MOCK_CONST_METHOD0(usage_statistics_reporting, bool());
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_SCREEN_OBSERVER_H_
