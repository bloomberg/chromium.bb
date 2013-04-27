// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_actor.h"
#include "chrome/browser/chromeos/login/screens/wizard_screen.h"
#include "chrome/browser/policy/cloud/cloud_policy_constants.h"

namespace chromeos {

class ScreenObserver;

// The screen implementation that links the enterprise enrollment UI into the
// OOBE wizard.
class EnrollmentScreen
    : public WizardScreen,
      public EnrollmentScreenActor::Controller {
 public:
  // Used in PyAuto testing.
  class TestingObserver {
   public:
    virtual ~TestingObserver() {}

    // Notifies observers of a change in enrollment state.
    virtual void OnEnrollmentComplete(bool succeeded) = 0;
  };

  EnrollmentScreen(ScreenObserver* observer,
                   EnrollmentScreenActor* actor);
  virtual ~EnrollmentScreen();

  void SetParameters(bool is_auto_enrollment,
                     bool can_exit_enrollment,
                     const std::string& enrollment_user);

  // WizardScreen implementation:
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  // EnrollmentScreenActor::Controller implementation:
  virtual void OnLoginDone(const std::string& user) OVERRIDE;
  virtual void OnAuthError(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthTokenAvailable(const std::string& oauth_token) OVERRIDE;
  virtual void OnRetry() OVERRIDE;
  virtual void OnCancel() OVERRIDE;
  virtual void OnConfirmationClosed() OVERRIDE;

  // Used for testing.
  EnrollmentScreenActor* GetActor() {
    return actor_;
  }

  // Used for testing.
  void AddTestingObserver(TestingObserver* observer);
  void RemoveTestingObserver(TestingObserver* observer);

 private:
  // Starts the Lockbox storage process.
  void WriteInstallAttributesData();

  // Kicks off the policy infrastructure to register with the service.
  void RegisterForDevicePolicy(const std::string& token);

  // Handles enrollment completion. Logs a UMA sample and requests the actor to
  // show the specified enrollment status.
  void ReportEnrollmentStatus(policy::EnrollmentStatus status);

  // Logs a UMA event in the kMetricEnrollment histogram. If auto-enrollment is
  // on |sample| is ignored and a kMetricEnrollmentAutoFailed sample is logged
  // instead.
  void UMAFailure(int sample);

  // Shows the signin screen. Used as a callback to run after auth reset.
  void ShowSigninScreen();

  // Notifies testing observers about the result of the enrollment.
  void NotifyTestingObservers(bool succeeded);

  EnrollmentScreenActor* actor_;
  bool is_auto_enrollment_;
  bool can_exit_enrollment_;
  bool enrollment_failed_once_;
  std::string user_;
  int lockbox_init_duration_;
  base::WeakPtrFactory<EnrollmentScreen> weak_ptr_factory_;

  // Observers.
  ObserverList<TestingObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(EnrollmentScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_ENROLLMENT_SCREEN_H_
