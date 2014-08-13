// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screens/screen_observer.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "policy/proto/device_management_backend.pb.h"

namespace chromeos {

EnrollmentScreen::EnrollmentScreen(
    ScreenObserver* observer,
    EnrollmentScreenActor* actor)
    : WizardScreen(observer),
      actor_(actor),
      enrollment_mode_(EnrollmentScreenActor::ENROLLMENT_MODE_MANUAL),
      enrollment_failed_once_(false),
      lockbox_init_duration_(0),
      weak_ptr_factory_(this) {
  // Init the TPM if it has not been done until now (in debug build we might
  // have not done that yet).
  DBusThreadManager::Get()->GetCryptohomeClient()->TpmCanAttemptOwnership(
      EmptyVoidDBusMethodCallback());
}

EnrollmentScreen::~EnrollmentScreen() {}

void EnrollmentScreen::SetParameters(
    EnrollmentScreenActor::EnrollmentMode enrollment_mode,
    const std::string& management_domain,
    const std::string& user) {
  enrollment_mode_ = enrollment_mode;
  user_ = user.empty() ? user : gaia::CanonicalizeEmail(user);
  actor_->SetParameters(this, enrollment_mode_, management_domain);
}

void EnrollmentScreen::PrepareToShow() {
  actor_->PrepareToShow();
}

void EnrollmentScreen::Show() {
  if (is_auto_enrollment() && !enrollment_failed_once_) {
    actor_->Show();
    UMA(policy::kMetricEnrollmentAutoStarted);
    actor_->ShowEnrollmentSpinnerScreen();
    actor_->FetchOAuthToken();
  } else {
    UMA(policy::kMetricEnrollmentTriggered);
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

  if (is_auto_enrollment())
    UMA(policy::kMetricEnrollmentAutoRetried);
  else if (enrollment_failed_once_)
    UMA(policy::kMetricEnrollmentRetried);
  else
    UMA(policy::kMetricEnrollmentStarted);

  actor_->ShowEnrollmentSpinnerScreen();
  actor_->FetchOAuthToken();
}

void EnrollmentScreen::OnAuthError(const GoogleServiceAuthError& error) {
  enrollment_failed_once_ = true;
  actor_->ShowAuthError(error);

  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
    case GoogleServiceAuthError::SERVICE_ERROR:
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

void EnrollmentScreen::OnOAuthTokenAvailable(const std::string& token) {
  RegisterForDevicePolicy(token);
}

void EnrollmentScreen::OnRetry() {
  actor_->ResetAuth(base::Bind(&EnrollmentScreen::ShowSigninScreen,
                               weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::OnCancel() {
  if (enrollment_mode_ == EnrollmentScreenActor::ENROLLMENT_MODE_FORCED ||
      enrollment_mode_ == EnrollmentScreenActor::ENROLLMENT_MODE_RECOVERY) {
    actor_->ResetAuth(
        base::Bind(&ScreenObserver::OnExit,
                   base::Unretained(get_screen_observer()),
                   ScreenObserver::ENTERPRISE_ENROLLMENT_BACK));
    return;
  }

  if (is_auto_enrollment())
    policy::AutoEnrollmentClient::CancelAutoEnrollment();
  UMA(is_auto_enrollment() ? policy::kMetricEnrollmentAutoCancelled
                           : policy::kMetricEnrollmentCancelled);
  actor_->ResetAuth(
      base::Bind(&ScreenObserver::OnExit,
                 base::Unretained(get_screen_observer()),
                 ScreenObserver::ENTERPRISE_ENROLLMENT_COMPLETED));
}

void EnrollmentScreen::OnConfirmationClosed() {
  // If the machine has been put in KIOSK mode we have to restart the session
  // here to go in the proper KIOSK mode login screen.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->GetDeviceMode() == policy::DEVICE_MODE_RETAIL_KIOSK) {
    DBusThreadManager::Get()->GetSessionManagerClient()->StopSession();
    return;
  }

  if (is_auto_enrollment() &&
      !enrollment_failed_once_ &&
      !user_.empty() &&
      LoginUtils::IsWhitelisted(user_, NULL)) {
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

void EnrollmentScreen::RegisterForDevicePolicy(const std::string& token) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged() &&
      connector->GetEnterpriseDomain() != gaia::ExtractDomainName(user_)) {
    LOG(ERROR) << "Trying to re-enroll to a different domain than "
               << connector->GetEnterpriseDomain();
    UMAFailure(policy::kMetricEnrollmentWrongUserError);
    actor_->ShowUIError(
        EnrollmentScreenActor::UI_ERROR_DOMAIN_MISMATCH);
    return;
  }

  policy::DeviceCloudPolicyInitializer::AllowedDeviceModes device_modes;
  device_modes[policy::DEVICE_MODE_ENTERPRISE] = true;
  device_modes[policy::DEVICE_MODE_RETAIL_KIOSK] =
      enrollment_mode_ == EnrollmentScreenActor::ENROLLMENT_MODE_MANUAL;
  connector->ScheduleServiceInitialization(0);

  policy::DeviceCloudPolicyInitializer* dcp_initializer =
      connector->GetDeviceCloudPolicyInitializer();
  CHECK(dcp_initializer);
  dcp_initializer->StartEnrollment(
      enterprise_management::PolicyData::ENTERPRISE_MANAGED,
      connector->device_management_service(),
      token, is_auto_enrollment(), device_modes,
      base::Bind(&EnrollmentScreen::ReportEnrollmentStatus,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::ShowEnrollmentStatusOnSuccess(
    const policy::EnrollmentStatus& status) {
  actor_->ShowEnrollmentStatus(status);
  StartupUtils::MarkOobeCompleted();
}

void EnrollmentScreen::ReportEnrollmentStatus(policy::EnrollmentStatus status) {
  if (status.status() == policy::EnrollmentStatus::STATUS_SUCCESS) {
    StartupUtils::MarkDeviceRegistered(
        base::Bind(&EnrollmentScreen::ShowEnrollmentStatusOnSuccess,
                   weak_ptr_factory_.GetWeakPtr(),
                   status));
    UMA(is_auto_enrollment() ? policy::kMetricEnrollmentAutoOK
                             : policy::kMetricEnrollmentOK);
    return;
  } else {
    enrollment_failed_once_ = true;
  }
  actor_->ShowEnrollmentStatus(status);

  switch (status.status()) {
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
        case policy::DM_STATUS_SERVICE_DEPROVISIONED:
          UMAFailure(policy::kMetricEnrollmentDeprovisioned);
          return;
        case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
          UMAFailure(policy::kMetricEnrollmentDomainMismatch);
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
    case policy::EnrollmentStatus::STATUS_NO_STATE_KEYS:
      UMAFailure(policy::kMetricEnrollmentNoStateKeys);
      return;
    case policy::EnrollmentStatus::STATUS_VALIDATION_FAILED:
      UMAFailure(policy::kMetricEnrollmentPolicyValidationFailed);
      return;
    case policy::EnrollmentStatus::STATUS_STORE_ERROR:
      UMAFailure(policy::kMetricEnrollmentCloudPolicyStoreError);
      return;
    case policy::EnrollmentStatus::STATUS_LOCK_ERROR:
      UMAFailure(policy::kMetricEnrollmentLockBackendError);
      return;
    case policy::EnrollmentStatus::STATUS_ROBOT_AUTH_FETCH_FAILED:
      UMAFailure(policy::kMetricEnrollmentRobotAuthCodeFetchFailed);
      return;
    case policy::EnrollmentStatus::STATUS_ROBOT_REFRESH_FETCH_FAILED:
      UMAFailure(policy::kMetricEnrollmentRobotRefreshTokenFetchFailed);
      return;
    case policy::EnrollmentStatus::STATUS_ROBOT_REFRESH_STORE_FAILED:
      UMAFailure(policy::kMetricEnrollmentRobotRefreshTokenStoreFailed);
      return;
    case policy::EnrollmentStatus::STATUS_STORE_TOKEN_AND_ID_FAILED:
      // This error should not happen for enterprise enrollment.
      UMAFailure(policy::kMetricEnrollmentStoreTokenAndIdFailed);
      NOTREACHED();
      return;
    case policy::EnrollmentStatus::STATUS_SUCCESS:
      NOTREACHED();
      return;
  }

  NOTREACHED();
  UMAFailure(policy::kMetricEnrollmentOtherFailed);
}

void EnrollmentScreen::UMA(policy::MetricEnrollment sample) {
  if (enrollment_mode_ == EnrollmentScreenActor::ENROLLMENT_MODE_RECOVERY) {
    UMA_HISTOGRAM_ENUMERATION(policy::kMetricEnrollmentRecovery, sample,
                              policy::kMetricEnrollmentSize);
  } else {
    UMA_HISTOGRAM_ENUMERATION(policy::kMetricEnrollment, sample,
                              policy::kMetricEnrollmentSize);
  }
}

void EnrollmentScreen::UMAFailure(policy::MetricEnrollment sample) {
  if (is_auto_enrollment())
    sample = policy::kMetricEnrollmentAutoFailed;
  UMA(sample);
}

void EnrollmentScreen::ShowSigninScreen() {
  actor_->Show();
  actor_->ShowSigninScreen();
}

}  // namespace chromeos
