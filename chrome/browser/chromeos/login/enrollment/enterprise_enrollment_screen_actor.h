// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/policy/enrollment_status_chromeos.h"

class GoogleServiceAuthError;

namespace chromeos {

// Interface class for the enterprise enrollment screen actor.
class EnterpriseEnrollmentScreenActor {
 public:
  // Enumeration of the possible errors that can occur during enrollment which
  // are not covered by GoogleServiceAuthError or EnrollmentStatus.
  enum UIError {
    // Existing enrollment domain doesn't match authentication user.
    UI_ERROR_DOMAIN_MISMATCH,
    // Requested device mode not supported with auto enrollment.
    UI_ERROR_AUTO_ENROLLMENT_BAD_MODE,
    // Unexpected error condition, indicates a bug in the code.
    UI_ERROR_FATAL,
  };

  // This defines the interface for controllers which will be called back when
  // something happens on the UI.
  class Controller {
   public:
    virtual ~Controller() {}

    virtual void OnLoginDone(const std::string& user) = 0;
    virtual void OnAuthError(const GoogleServiceAuthError& error) = 0;
    virtual void OnOAuthTokenAvailable(const std::string& oauth_token) = 0;
    virtual void OnRetry() = 0;
    virtual void OnCancel() = 0;
    virtual void OnConfirmationClosed() = 0;
  };

  virtual ~EnterpriseEnrollmentScreenActor() {}

  // Initializes the actor with parameters.
  virtual void SetParameters(Controller* controller,
                             bool is_auto_enrollment,
                             const std::string& user) = 0;

  // Prepare the contents to showing.
  virtual void PrepareToShow() = 0;

  // Shows the contents of the screen.
  virtual void Show() = 0;

  // Hides the contents of the screen.
  virtual void Hide() = 0;

  // Starts fetching the OAuth token.
  virtual void FetchOAuthToken() = 0;

  // Resets the authentication state and invokes the passed callback on
  // completion.
  virtual void ResetAuth(const base::Closure& callback) = 0;

  // Shows the signin screen.
  virtual void ShowSigninScreen() = 0;

  // Shows the spinner screen for enrollment.
  virtual void ShowEnrollmentSpinnerScreen() = 0;

  // Shows the spinner screen for login after auto-enrollment.
  virtual void ShowLoginSpinnerScreen() = 0;

  // Show an authentication error.
  virtual void ShowAuthError(const GoogleServiceAuthError& error) = 0;

  // Show non-authentication error.
  virtual void ShowUIError(UIError error) = 0;

  // Update the UI to report the |status| of the enrollment procedure.
  virtual void ShowEnrollmentStatus(policy::EnrollmentStatus status) = 0;

  // Used for testing only.
  virtual void SubmitTestCredentials(const std::string& email,
                                     const std::string& password) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENTERPRISE_ENROLLMENT_SCREEN_ACTOR_H_
