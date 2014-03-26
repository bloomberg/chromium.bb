// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_actor.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockEnrollmentScreen : public EnrollmentScreen {
 public:
  MockEnrollmentScreen(ScreenObserver* screen_observer,
                       EnrollmentScreenActor* actor);
  virtual ~MockEnrollmentScreen();
};

class MockEnrollmentScreenActor
    : public EnrollmentScreenActor {
 public:
  MockEnrollmentScreenActor();
  virtual ~MockEnrollmentScreenActor();

  MOCK_METHOD3(SetParameters,
               void(Controller*,
                    EnrollmentScreenActor::EnrollmentMode,
                    const std::string&));
  MOCK_METHOD0(PrepareToShow, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(FetchOAuthToken, void());
  MOCK_METHOD1(ResetAuth, void(const base::Closure&));
  MOCK_METHOD0(ShowSigninScreen, void());
  MOCK_METHOD0(ShowEnrollmentSpinnerScreen, void());
  MOCK_METHOD0(ShowLoginSpinnerScreen, void());
  MOCK_METHOD1(ShowAuthError, void(const GoogleServiceAuthError&));
  MOCK_METHOD1(ShowUIError, void(UIError));
  MOCK_METHOD1(ShowEnrollmentStatus, void(policy::EnrollmentStatus status));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENROLLMENT_SCREEN_H_
