// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen_actor.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_controller.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"

namespace chromeos {

class BaseScreenDelegate;
class ErrorScreensHistogramHelper;
class ScreenManager;

// Handles the control flow after OOBE auto-update completes to wait for the
// enterprise auto-enrollment check that happens as part of OOBE. This includes
// keeping track of current auto-enrollment state and displaying and updating
// the error screen upon failures. Similar to a screen controller, but it
// doesn't actually drive a dedicated screen.
class AutoEnrollmentCheckScreen
    : public AutoEnrollmentCheckScreenActor::Delegate,
      public BaseScreen,
      public NetworkPortalDetector::Observer {
 public:
  AutoEnrollmentCheckScreen(BaseScreenDelegate* base_screen_delegate,
                            AutoEnrollmentCheckScreenActor* actor);
  virtual ~AutoEnrollmentCheckScreen();

  static AutoEnrollmentCheckScreen* Get(ScreenManager* manager);

  // Hands over OOBE control to this AutoEnrollmentCheckStep. It'll return the
  // flow back to the caller via the |base_screen_delegate_|'s OnExit function.
  void Start();

  // Clears the cached state causing the forced enrollment check to be retried.
  void ClearState();

  void set_auto_enrollment_controller(
      AutoEnrollmentController* auto_enrollment_controller) {
    auto_enrollment_controller_ = auto_enrollment_controller;
  }

  // BaseScreen implementation:
  virtual void PrepareToShow() override;
  virtual void Show() override;
  virtual void Hide() override;
  virtual std::string GetName() const override;

  // AutoEnrollmentCheckScreenActor::Delegate implementation:
  virtual void OnExit() override;
  virtual void OnActorDestroyed(AutoEnrollmentCheckScreenActor* actor) override;

  // NetworkPortalDetector::Observer implementation:
  virtual void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

 private:
  // Handles update notifications regarding the auto-enrollment check.
  void OnAutoEnrollmentCheckProgressed(policy::AutoEnrollmentState state);

  // Handles a state update, updating the UI and saving the state.
  void UpdateState(
      NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status,
      policy::AutoEnrollmentState new_auto_enrollment_state);

  // Configures the UI to reflect |new_captive_portal_status|. Returns true if
  // and only if a UI change has been made.
  bool UpdateCaptivePortalStatus(
      NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status);

  // Configures the UI to reflect |auto_enrollment_state|. Returns true if and
  // only if a UI change has been made.
  bool UpdateAutoEnrollmentState(
      policy::AutoEnrollmentState auto_enrollment_state);

  // Configures the error screen.
  void ShowErrorScreen(ErrorScreen::ErrorState error_state);

  // Signals completion. No further code should run after a call to this
  // function as the owner might destroy |this| in response.
  void SignalCompletion();

  // Checks if the enrollment status check is needed. It can be disabled either
  // by command line flags, build configuration or might have finished already.
  bool IsStartNeeded();

  AutoEnrollmentCheckScreenActor* actor_;
  AutoEnrollmentController* auto_enrollment_controller_;

  scoped_ptr<AutoEnrollmentController::ProgressCallbackList::Subscription>
      auto_enrollment_progress_subscription_;

  NetworkPortalDetector::CaptivePortalStatus captive_portal_status_;
  policy::AutoEnrollmentState auto_enrollment_state_;

  scoped_ptr<ErrorScreensHistogramHelper> histogram_helper_;

  DISALLOW_COPY_AND_ASSIGN(AutoEnrollmentCheckScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CHECK_SCREEN_H_
