// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENTERPRISE_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENTERPRISE_ENROLLMENT_SCREEN_H_

#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen_actor.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockEnterpriseEnrollmentScreen : public EnterpriseEnrollmentScreen {
 public:
  MockEnterpriseEnrollmentScreen(ScreenObserver* screen_observer,
                                 EnterpriseEnrollmentScreenActor* actor);
  virtual ~MockEnterpriseEnrollmentScreen();
};

class MockEnterpriseEnrollmentScreenActor
    : public EnterpriseEnrollmentScreenActor {
 public:
  MockEnterpriseEnrollmentScreenActor();
  virtual ~MockEnterpriseEnrollmentScreenActor();

  MOCK_METHOD1(SetController,
               void(EnterpriseEnrollmentScreenActor::Controller* controller));
  MOCK_METHOD0(PrepareToShow, void());
  MOCK_METHOD0(Show, void());
  MOCK_METHOD0(Hide, void());
  MOCK_METHOD0(ShowConfirmationScreen, void());
  MOCK_METHOD1(ShowAuthError, void(const GoogleServiceAuthError& error));
  MOCK_METHOD1(ShowEnrollmentError, void(EnrollmentError error_code));
  MOCK_METHOD2(SubmitTestCredentials, void(const std::string& email,
                                           const std::string& password));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_MOCK_ENTERPRISE_ENROLLMENT_SCREEN_H_
