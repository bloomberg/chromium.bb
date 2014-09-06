// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {

// static
AutoEnrollmentCheckScreen* AutoEnrollmentCheckScreen::Get(
    ScreenManager* manager) {
  return static_cast<AutoEnrollmentCheckScreen*>(
      manager->GetScreen(WizardController::kAutoEnrollmentCheckScreenName));
}

AutoEnrollmentCheckScreen::AutoEnrollmentCheckScreen(
    ScreenObserver* observer,
    AutoEnrollmentCheckScreenActor* actor)
    : WizardScreen(observer),
      actor_(actor),
      captive_portal_status_(
          NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN),
      auto_enrollment_state_(policy::AUTO_ENROLLMENT_STATE_IDLE) {
  if (actor_)
    actor_->SetDelegate(this);
}

AutoEnrollmentCheckScreen::~AutoEnrollmentCheckScreen() {
  NetworkPortalDetector::Get()->RemoveObserver(this);
  if (actor_)
    actor_->SetDelegate(NULL);
}

void AutoEnrollmentCheckScreen::Start() {
  if (!IsStartNeeded())
    return;

  // Make sure the auto-enrollment client is running.
  auto_enrollment_controller_->Start();

  auto_enrollment_progress_subscription_ =
      auto_enrollment_controller_->RegisterProgressCallback(
          base::Bind(
              &AutoEnrollmentCheckScreen::OnAutoEnrollmentCheckProgressed,
              base::Unretained(this)));
  auto_enrollment_state_ = auto_enrollment_controller_->state();

  // NB: AddAndFireObserver below call back into OnPortalDetectionCompleted.
  // This guarantees that the UI gets synced to current state.
  NetworkPortalDetector* portal_detector = NetworkPortalDetector::Get();
  portal_detector->StartDetectionIfIdle();
  portal_detector->AddAndFireObserver(this);
}

bool AutoEnrollmentCheckScreen::IsStartNeeded() {
  // Check that forced reenrollment is wanted and if the check is needed or we
  // already know the outcome.
  if (AutoEnrollmentController::GetMode() !=
      AutoEnrollmentController::MODE_FORCED_RE_ENROLLMENT ||
      auto_enrollment_state_ ==
      policy::AUTO_ENROLLMENT_STATE_TRIGGER_ENROLLMENT ||
      auto_enrollment_state_ == policy::AUTO_ENROLLMENT_STATE_NO_ENROLLMENT) {
    SignalCompletion();
    return false;
  }
  return true;
}

void AutoEnrollmentCheckScreen::PrepareToShow() {
}

void AutoEnrollmentCheckScreen::Show() {
  if (IsStartNeeded()) {
    Start();
    if (actor_)
      actor_->Show();
  }
}

void AutoEnrollmentCheckScreen::Hide() {
}

std::string AutoEnrollmentCheckScreen::GetName() const {
  return WizardController::kAutoEnrollmentCheckScreenName;
}

void AutoEnrollmentCheckScreen::OnExit() {
  get_screen_observer()->OnExit(
      ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);
}

void AutoEnrollmentCheckScreen::OnActorDestroyed(
    AutoEnrollmentCheckScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

void AutoEnrollmentCheckScreen::OnPortalDetectionCompleted(
    const NetworkState* /* network */,
    const NetworkPortalDetector::CaptivePortalState& state) {
  UpdateState(state.status, auto_enrollment_state_);
}

void AutoEnrollmentCheckScreen::OnAutoEnrollmentCheckProgressed(
    policy::AutoEnrollmentState state) {
  UpdateState(captive_portal_status_, state);
}

void AutoEnrollmentCheckScreen::UpdateState(
    NetworkPortalDetector::CaptivePortalStatus new_captive_portal_status,
    policy::AutoEnrollmentState new_auto_enrollment_state) {
  // Configure the error screen to show the approriate error message.
  if (!UpdateCaptivePortalStatus(new_captive_portal_status))
    UpdateAutoEnrollmentState(new_auto_enrollment_state);

  // Update the connecting indicator.
  ErrorScreen* error_screen = get_screen_observer()->GetErrorScreen();
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

bool AutoEnrollmentCheckScreen::UpdateCaptivePortalStatus(
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
        get_screen_observer()->GetErrorScreen()->FixCaptivePortal();
      return true;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      ShowErrorScreen(ErrorScreen::ERROR_STATE_PROXY);
      return true;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_COUNT:
      NOTREACHED() << "Bad status: CAPTIVE_PORTAL_STATUS_COUNT";
      return false;
  }

  // Return is required to avoid compiler warning.
  NOTREACHED() << "Bad status " << new_captive_portal_status;
  return false;
}

bool AutoEnrollmentCheckScreen::UpdateAutoEnrollmentState(
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

  // Return is required to avoid compiler warning.
  NOTREACHED() << "bad state " << new_auto_enrollment_state;
  return false;
}

void AutoEnrollmentCheckScreen::ShowErrorScreen(
    ErrorScreen::ErrorState error_state) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  ErrorScreen* error_screen = get_screen_observer()->GetErrorScreen();
  error_screen->SetUIState(ErrorScreen::UI_STATE_AUTO_ENROLLMENT_ERROR);
  error_screen->AllowGuestSignin(true);
  error_screen->SetErrorState(error_state,
                              network ? network->name() : std::string());
  get_screen_observer()->ShowErrorScreen();
}

void AutoEnrollmentCheckScreen::SignalCompletion() {
  NetworkPortalDetector::Get()->RemoveObserver(this);
  auto_enrollment_progress_subscription_.reset();
  get_screen_observer()->OnExit(
      ScreenObserver::ENTERPRISE_AUTO_ENROLLMENT_CHECK_COMPLETED);
}

}  // namespace chromeos
