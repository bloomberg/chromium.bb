// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_

#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen_actor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockAutoEnrollmentCheckScreen : public AutoEnrollmentCheckScreen {
 public:
  MockAutoEnrollmentCheckScreen(
      ScreenObserver* screen_observer,
      AutoEnrollmentCheckScreenActor* actor);
  virtual ~MockAutoEnrollmentCheckScreen();
};

class MockAutoEnrollmentCheckScreenActor
    : public AutoEnrollmentCheckScreenActor {
 public:
  MockAutoEnrollmentCheckScreenActor();
  virtual ~MockAutoEnrollmentCheckScreenActor();

  virtual void SetDelegate(Delegate* screen) OVERRIDE;

  MOCK_METHOD1(MockSetDelegate, void(Delegate* screen));
  MOCK_METHOD0(Show, void());

 private:
  Delegate* screen_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_AUTO_ENROLLMENT_CHECK_SCREEN_H_
