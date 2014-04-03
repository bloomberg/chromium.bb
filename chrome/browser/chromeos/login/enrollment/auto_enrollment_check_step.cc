// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_step.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

AutoEnrollmentCheckStep::AutoEnrollmentCheckStep(
    ScreenObserver* screen_observer,
    AutoEnrollmentController* auto_enrollment_controller)
    : screen_observer_(screen_observer),
      auto_enrollment_controller_(auto_enrollment_controller),
      captive_portal_status_(
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN),
      auto_enrollment_state_(policy::AUTO_ENROLLMENT_STATE_IDLE) {}

AutoEnrollmentCheckStep::~AutoEnrollmentCheckStep() {
  NetworkPortalDetector::Get()->RemoveObserver(this);
}

void AutoEnrollmentCheckStep::Start() {
  if (AutoEnrollmentController::GetMode() !=
      AutoEnrollmentController::MODE_FORCED_RE_ENROLLMENT) {
    SignalCompletion();
    return;
  }

  // Make sure the auto-enrollment client is running.
  auto_enrollment_controller_->Start();

  auto_enrollment_progress_subscription_ =
      auto_enrollment_controller_->RegisterProgressCallback(
          base::Bind(&AutoEnrollmentCheckStep::OnAutoEnrollmentCheckProgressed,
                     base::Unretained(this)));
  auto_enrollment_state_ = auto_enrollment_controller_->state();

  // NB: AddAndFireObserver below call back into OnPortalDetectionCompleted.
  // This guarantees that the UI gets synced to current state.
  NetworkPortalDetector* portal_detector = NetworkPortalDetector::Get();
  portal_detector->StartDetectionIfIdle();
  portal_detector->AddAndFireObserver(this);
}

void AutoEnrollmentCheckStep::OnPortalDetectionCompleted(
    const NetworkState* /* network */,
    const NetworkPortalDetector::CaptivePortalState& state) {
  UpdateState(state.status, auto_enrollment_state_);
}

void AutoEnrollmentCheckStep::OnAutoEnrollmentCheckProgressed(
    policy::AutoEnrollmentState state) {
  UpdateState(captive_portal_status_, state);
}

void AutoEnrollmentCheckStep::UpdateState(
    NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status,
    policy::AutoEnrollmentState new_auto_enrollment_state) {
  // Configure the error screen to show the approriate error message.
  if (!UpdateCaptivePortalStatus(new_captive_portal_status))
    UpdateAutoEnrollmentState(new_auto_enrollment_state);

  // Update the connecting indicator.
  ErrorScreen* error_screen = screen_observer_->GetErrorScreen();
  error_screen->ShowConnectingIndicator(
      new_auto_enrollment_state == policy::AUTO_ENROLLMENT_STATE_PENDING);

  // Determine whether a retry is in order.
  bool retry = (new_captive_portal_status ==
                NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) &&
               (captive_portal_status_ !=
                NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE);

  // Save the new state.
  captive_portal_status_ = new_captive_portal_status;
  auto_enrollment_state_ = new_auto_enrollment_state;

  // Check whether a decision got made.
  switch (new_auto_enrollment_state) {
    case policy::AUTO_ENROLLMENT_STATE_IDLE:
      NOTREACHED();
      // fall through.
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
      break;
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
      // Server errors don't block OOBE.
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
      // Decision made, ready to proceed.
      SignalCompletion();
      return;
  }

  // Retry if applicable. This is last so eventual callbacks find consistent
  // state.
  if (retry)
    auto_enrollment_controller_->Retry();
}

bool AutoEnrollmentCheckStep::UpdateCaptivePortalStatus(
    NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status) {
  switch (new_captive_portal_status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      return false;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      ShowErrorScreen(ErrorScreen::ERROR_STATE_OFFLINE);
      return true;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      ShowErrorScreen(ErrorScreen::ERROR_STATE_PORTAL);
      if (captive_portal_status_ != new_captive_portal_status)
        screen_observer_->GetErrorScreen()->FixCaptivePortal();
      return true;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      ShowErrorScreen(ErrorScreen::ERROR_STATE_PROXY);
      return true;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT:
      // Trigger NOTREACHED() below.
      break;
  }

  NOTREACHED() << "Bad status " << new_captive_portal_status;
  return false;
}

bool AutoEnrollmentCheckStep::UpdateAutoEnrollmentState(
    policy::AutoEnrollmentState new_auto_enrollment_state) {
  switch (new_auto_enrollment_state) {
    case policy::AUTO_ENROLLMENT_STATE_IDLE:
      // The client should have been started already.
      NOTREACHED();
      return false;
    case policy::AUTO_ENROLLMENT_STATE_PENDING:
    case policy::AUTO_ENROLLMENT_STATE_SERVER_ERROR:
    case policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT:
    case policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT:
      return false;
    case policy::AUTO_ENROLLMENT_STATE_CONNECTION_ERROR:
      ShowErrorScreen(ErrorScreen::ERROR_STATE_OFFLINE);
      return true;
  }

  NOTREACHED() << "bad state " << new_auto_enrollment_state;
  return false;
}

void AutoEnrollmentCheckStep::ShowErrorScreen(
    ErrorScreen::ErrorState error_state) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  ErrorScreen* error_screen = screen_observer_->GetErrorScreen();
  error_screen->SetUIState(ErrorScreen::UI_STATE_AUTO_ENROLLMENT_ERROR);
  error_screen->AllowGuestSignin(true);
  error_screen->SetErrorState(error_state,
                              network ? network->name() : std::string());
  screen_observer_->ShowErrorScreen();
}

void AutoEnrollmentCheckStep::SignalCompletion() {
  NetworkPortalDetector::Get()->RemoveObserver(this);
  auto_enrollment_progress_subscription_.reset();
  screen_observer_->OnExit(
      ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);
}

}  // namespace chromeos
