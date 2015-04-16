// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_ACTOR_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_helper.h"

class GoogleServiceAuthError;

namespace policy {
struct EnrollmentConfig;
class EnrollmentStatus;
}

namespace chromeos {

// Interface class for the enterprise enrollment screen actor.
class EnrollmentScreenActor {
 public:
  // This defines the interface for controllers which will be called back when
  // something happens on the UI.
  class Controller {
   public:
    virtual ~Controller() {}

    virtual void OnLoginDone(const std::string& user,
                             const std::string& auth_code) = 0;
    virtual void OnRetry() = 0;
    virtual void OnCancel() = 0;
    virtual void OnConfirmationClosed() = 0;
    virtual void OnDeviceAttributeProvided(const std::string& asset_id,
                                           const std::string& location) = 0;
  };

  virtual ~EnrollmentScreenActor() {}

  // Initializes the actor with parameters.
  virtual void SetParameters(Controller* controller,
                             const policy::EnrollmentConfig& config) = 0;

  // Prepare the contents to showing.
  virtual void PrepareToShow() = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Shows the signin screen.
  virtual void ShowSigninScreen() = 0;

  // Shows the device attribute prompt screen.
  virtual void ShowAttributePromptScreen(const std::string& asset_id,
                                         const std::string& location) = 0;

  // Shows the spinner screen for enrollment.
  virtual void ShowEnrollmentSpinnerScreen() = 0;

  // Show an authentication error.
  virtual void ShowAuthError(const GoogleServiceAuthError& error) = 0;

  // Show non-authentication error.
  virtual void ShowOtherError(EnterpriseEnrollmentHelper::OtherError error) = 0;

  // Update the UI to report the |status| of the enrollment procedure.
  virtual void ShowEnrollmentStatus(policy::EnrollmentStatus status) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_ACTOR_H_
