// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/cloud/enterprise_metrics.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace chromeos {

namespace {

void UMA(int sample) {
  UMA_HISTOGRAM_ENUMERATION(policy::kMetricEnrollment,
                            sample,
                            policy::kMetricEnrollmentSize);
}

// Does nothing.  Used as a VoidDBusMethodCallback.
void EmptyVoidDBusMethodCallback(DBusMethodCallStatus result) {}

}  // namespace

EnrollmentScreen::EnrollmentScreen(
    ScreenObserver* observer,
    EnrollmentScreenActor* actor)
    : WizardScreen(observer),
      actor_(actor),
      is_auto_enrollment_(false),
      can_exit_enrollment_(true),
      enrollment_failed_once_(false),
      lockbox_init_duration_(0),
      weak_ptr_factory_(this) {
  // Init the TPM if it has not been done until now (in debug build we might
  // have not done that yet).
  DBusThreadManager::Get()->GetCryptohomeClient()->TpmCanAttemptOwnership(
      base::Bind(&EmptyVoidDBusMethodCallback));
}

EnrollmentScreen::~EnrollmentScreen() {}

void EnrollmentScreen::SetParameters(bool is_auto_enrollment,
                                     bool can_exit_enrollment,
                                     const std::string& user) {
  is_auto_enrollment_ = is_auto_enrollment;
  can_exit_enrollment_ = can_exit_enrollment;
  user_ = user.empty() ? user : gaia::CanonicalizeEmail(user);
  actor_->SetParameters(this, is_auto_enrollment_, can_exit_enrollment, user_);
}

void EnrollmentScreen::PrepareToShow() {
  actor_->PrepareToShow();
}

void EnrollmentScreen::Show() {
  if (is_auto_enrollment_ && !enrollment_failed_once_) {
    actor_->Show();
    UMA(policy::kMetricEnrollmentAutoStarted);
    actor_->ShowEnrollmentSpinnerScreen();
    actor_->FetchOAuthToken();
  } else {
    actor_->ResetAuth(base::Bind(&EnrollmentScreen::ShowSigninScreen,
                                 weak_ptr_factory_.GetWeakPtr()));
  }
}

void EnrollmentScreen::Hide() {
  actor_->Hide();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

std::string EnrollmentScreen::GetName() const {
  return WizardController::kEnrollmentScreenName;
}

void EnrollmentScreen::OnLoginDone(const std::string& user) {
  user_ = gaia::CanonicalizeEmail(user);

  UMA(is_auto_enrollment_ ? policy::kMetricEnrollmentAutoRetried
                          : policy::kMetricEnrollmentStarted);

  actor_->ShowEnrollmentSpinnerScreen();
  actor_->FetchOAuthToken();
}

void EnrollmentScreen::OnAuthError(
    const GoogleServiceAuthError& error) {
  enrollment_failed_once_ = true;
  actor_->ShowAuthError(error);
  NotifyTestingObservers(false);

  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
      UMAFailure(policy::kMetricEnrollmentLoginFailed);
      LOG(ERROR) << "Auth error " << error.state();
      return;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      UMAFailure(policy::kMetricEnrollmentNotSupported);
      LOG(ERROR) << "Account error " << error.state();
      return;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      UMAFailure(policy::kMetricEnrollmentNetworkFailed);
      LOG(WARNING) << "Network error " << error.state();
      return;
    case GoogleServiceAuthError::NUM_STATES:
      break;
  }

  NOTREACHED();
  UMAFailure(policy::kMetricEnrollmentOtherFailed);
}

void EnrollmentScreen::OnOAuthTokenAvailable(
    const std::string& token) {
  RegisterForDevicePolicy(token);
}

void EnrollmentScreen::OnRetry() {
  actor_->ResetAuth(base::Bind(&EnrollmentScreen::ShowSigninScreen,
                               weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::OnCancel() {
  if (!can_exit_enrollment_) {
    NOTREACHED() << "Cancellation should not be permitted";
    return;
  }

  if (is_auto_enrollment_)
    policy::AutoEnrollmentClient::CancelAutoEnrollment();
  UMA(is_auto_enrollment_ ? policy::kMetricEnrollmentAutoCancelled
                          : policy::kMetricEnrollmentCancelled);
  actor_->ResetAuth(
      base::Bind(&ScreenObserver::OnExit,
                 base::Unretained(get_screen_observer()),
                 ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED));
  NotifyTestingObservers(false);
}

void EnrollmentScreen::OnConfirmationClosed() {
  // If the machine has been put in KIOSK mode we have to restart the session
  // here to go in the proper KIOSK mode login screen.
  if (g_browser_process->browser_policy_connector()->GetDeviceMode() ==
          policy::DEVICE_MODE_KIOSK) {
    DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();
    return;
  }

  if (is_auto_enrollment_ &&
      !enrollment_failed_once_ &&
      !user_.empty() &&
      LoginUtils::IsWhitelisted(user_)) {
    actor_->ShowLoginSpinnerScreen();
    get_screen_observer()->OnExit(
        ScreenObserver::ENTERPRISE_AUTO_MAGIC_ENROLLMENT_COMPLETED);
  } else {
    actor_->ResetAuth(
        base::Bind(&ScreenObserver::OnExit,
                   base::Unretained(get_screen_observer()),
                   ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED));
  }
}

void EnrollmentScreen::AddTestingObserver(TestingObserver* observer) {
  observers_.AddObserver(observer);
}

void EnrollmentScreen::RemoveTestingObserver(
    TestingObserver* observer) {
  observers_.RemoveObserver(observer);
}

void EnrollmentScreen::RegisterForDevicePolicy(
    const std::string& token) {
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  if (connector->IsEnterpriseManaged() &&
      connector->GetEnterpriseDomain() != gaia::ExtractDomainName(user_)) {
    LOG(ERROR) << "Trying to re-enroll to a different domain than "
               << connector->GetEnterpriseDomain();
    UMAFailure(policy::kMetricEnrollmentWrongUserError);
    actor_->ShowUIError(
        EnrollmentScreenActor::UI_ERROR_DOMAIN_MISMATCH);
    NotifyTestingObservers(false);
    return;
  }

  policy::DeviceCloudPolicyManagerChromeOS::AllowedDeviceModes modes;
  modes[policy::DEVICE_MODE_ENTERPRISE] = true;
  modes[policy::DEVICE_MODE_KIOSK] = !is_auto_enrollment_;
  connector->ScheduleServiceInitialization(0);
  connector->GetDeviceCloudPolicyManager()->StartEnrollment(
      token, is_auto_enrollment_, modes,
      base::Bind(&EnrollmentScreen::ReportEnrollmentStatus,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::ReportEnrollmentStatus(
    policy::EnrollmentStatus status) {
  bool success = status.status() == policy::EnrollmentStatus::STATUS_SUCCESS;
  enrollment_failed_once_ |= !success;
  actor_->ShowEnrollmentStatus(status);
  NotifyTestingObservers(success);

  switch (status.status()) {
    case policy::EnrollmentStatus::STATUS_SUCCESS:
      StartupUtils::MarkDeviceRegistered();
      UMA(is_auto_enrollment_ ? policy::kMetricEnrollmentAutoOK
                              : policy::kMetricEnrollmentOK);
      return;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_FAILED:
    case policy::EnrollmentStatus::STATUS_POLICY_FETCH_FAILED:
      switch (status.client_status()) {
        case policy::DM_STATUS_SUCCESS:
        case policy::DM_STATUS_REQUEST_INVALID:
        case policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
        case policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
        case policy::DM_STATUS_SERVICE_ACTIVATION_PENDING:
        case policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
        case policy::DM_STATUS_SERVICE_POLICY_NOT_FOUND:
          UMAFailure(policy::kMetricEnrollmentOtherFailed);
          return;
        case policy::DM_STATUS_REQUEST_FAILED:
        case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:
        case policy::DM_STATUS_HTTP_STATUS_ERROR:
        case policy::DM_STATUS_RESPONSE_DECODING_ERROR:
          UMAFailure(policy::kMetricEnrollmentNetworkFailed);
          return;
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          UMAFailure(policy::kMetricEnrollmentNotSupported);
          return;
        case policy::DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
          UMAFailure(policy::kMetricEnrollmentInvalidSerialNumber);
          return;
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          UMAFailure(policy::kMetricMissingLicensesError);
          return;
      }
      break;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_BAD_MODE:
      UMAFailure(policy::kMetricEnrollmentInvalidEnrollmentMode);
      return;
    case policy::EnrollmentStatus::STATUS_LOCK_TIMEOUT:
      UMAFailure(policy::kMetricLockboxTimeoutError);
      return;
    case policy::EnrollmentStatus::STATUS_LOCK_WRONG_USER:
      UMAFailure(policy::kMetricEnrollmentWrongUserError);
      return;
    case policy::EnrollmentStatus::STATUS_VALIDATION_FAILED:
    case policy::EnrollmentStatus::STATUS_STORE_ERROR:
    case policy::EnrollmentStatus::STATUS_LOCK_ERROR:
      UMAFailure(policy::kMetricEnrollmentOtherFailed);
      return;
  }

  NOTREACHED();
  UMAFailure(policy::kMetricEnrollmentOtherFailed);
}

void EnrollmentScreen::UMAFailure(int sample) {
  if (is_auto_enrollment_)
    sample = policy::kMetricEnrollmentAutoFailed;
  UMA(sample);
}

void EnrollmentScreen::ShowSigninScreen() {
  actor_->Show();
  actor_->ShowSigninScreen();
}

void EnrollmentScreen::NotifyTestingObservers(bool succeeded) {
  FOR_EACH_OBSERVER(TestingObserver, observers_,
                    OnEnrollmentComplete(succeeded));
}

}  // namespace chromeos
