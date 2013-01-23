// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enterprise_enrollment_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_observer.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/policy/auto_enrollment_client.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/policy/enterprise_metrics.h"
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

}  // namespace

EnterpriseEnrollmentScreen::EnterpriseEnrollmentScreen(
    ScreenObserver* observer,
    EnterpriseEnrollmentScreenActor* actor)
    : WizardScreen(observer),
      actor_(actor),
      is_auto_enrollment_(false),
      enrollment_failed_once_(false),
      lockbox_init_duration_(0),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  // Init the TPM if it has not been done until now (in debug build we might
  // have not done that yet).
  chromeos::CryptohomeLibrary* cryptohome =
      chromeos::CrosLibrary::Get()->GetCryptohomeLibrary();
  if (cryptohome &&
      cryptohome->TpmIsEnabled() &&
      !cryptohome->TpmIsBeingOwned() &&
      !cryptohome->TpmIsOwned()) {
    cryptohome->TpmCanAttemptOwnership();
  }
}

EnterpriseEnrollmentScreen::~EnterpriseEnrollmentScreen() {}

void EnterpriseEnrollmentScreen::SetParameters(bool is_auto_enrollment,
                                               const std::string& user) {
  is_auto_enrollment_ = is_auto_enrollment;
  user_ = user.empty() ? user : gaia::CanonicalizeEmail(user);
  actor_->SetParameters(this, is_auto_enrollment_, user_);
}

void EnterpriseEnrollmentScreen::PrepareToShow() {
  actor_->PrepareToShow();
}

void EnterpriseEnrollmentScreen::Show() {
  if (is_auto_enrollment_ && !enrollment_failed_once_) {
    actor_->Show();
    UMA(policy::kMetricEnrollmentAutoStarted);
    actor_->ShowEnrollmentSpinnerScreen();
    actor_->FetchOAuthToken();
  } else {
    actor_->ResetAuth(base::Bind(&EnterpriseEnrollmentScreen::ShowSigninScreen,
                                 weak_ptr_factory_.GetWeakPtr()));
  }
}

void EnterpriseEnrollmentScreen::Hide() {
  actor_->Hide();
  weak_ptr_factory_.InvalidateWeakPtrs();
}

std::string EnterpriseEnrollmentScreen::GetName() const {
  return WizardController::kEnterpriseEnrollmentScreenName;
}

void EnterpriseEnrollmentScreen::OnLoginDone(const std::string& user) {
  user_ = gaia::CanonicalizeEmail(user);

  UMA(is_auto_enrollment_ ? policy::kMetricEnrollmentAutoRetried
                          : policy::kMetricEnrollmentStarted);

  actor_->ShowEnrollmentSpinnerScreen();
  actor_->FetchOAuthToken();
}

void EnterpriseEnrollmentScreen::OnAuthError(
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

void EnterpriseEnrollmentScreen::OnOAuthTokenAvailable(
    const std::string& token) {
  RegisterForDevicePolicy(token);
}

void EnterpriseEnrollmentScreen::OnRetry() {
  actor_->ResetAuth(base::Bind(&EnterpriseEnrollmentScreen::ShowSigninScreen,
                               weak_ptr_factory_.GetWeakPtr()));
}

void EnterpriseEnrollmentScreen::OnCancel() {
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

void EnterpriseEnrollmentScreen::OnConfirmationClosed() {
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

void EnterpriseEnrollmentScreen::AddTestingObserver(TestingObserver* observer) {
  observers_.AddObserver(observer);
}

void EnterpriseEnrollmentScreen::RemoveTestingObserver(
    TestingObserver* observer) {
  observers_.RemoveObserver(observer);
}

void EnterpriseEnrollmentScreen::RegisterForDevicePolicy(
    const std::string& token) {
  policy::BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  if (connector->IsEnterpriseManaged() &&
      connector->GetEnterpriseDomain() != gaia::ExtractDomainName(user_)) {
    LOG(ERROR) << "Trying to re-enroll to a different domain than "
               << connector->GetEnterpriseDomain();
    UMAFailure(policy::kMetricEnrollmentWrongUserError);
    actor_->ShowUIError(
        EnterpriseEnrollmentScreenActor::UI_ERROR_DOMAIN_MISMATCH);
    NotifyTestingObservers(false);
    return;
  }

  policy::DeviceCloudPolicyManagerChromeOS::AllowedDeviceModes modes;
  modes[policy::DEVICE_MODE_ENTERPRISE] = true;
  modes[policy::DEVICE_MODE_KIOSK] = !is_auto_enrollment_;
  connector->ScheduleServiceInitialization(0);
  connector->GetDeviceCloudPolicyManager()->StartEnrollment(
      token, is_auto_enrollment_, modes,
      base::Bind(&EnterpriseEnrollmentScreen::ReportEnrollmentStatus,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EnterpriseEnrollmentScreen::ReportEnrollmentStatus(
    policy::EnrollmentStatus status) {
  bool success = status.status() == policy::EnrollmentStatus::STATUS_SUCCESS;
  enrollment_failed_once_ |= !success;
  actor_->ShowEnrollmentStatus(status);
  NotifyTestingObservers(success);

  switch (status.status()) {
    case policy::EnrollmentStatus::STATUS_SUCCESS:
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

void EnterpriseEnrollmentScreen::UMAFailure(int sample) {
  if (is_auto_enrollment_)
    sample = policy::kMetricEnrollmentAutoFailed;
  UMA(sample);
}

void EnterpriseEnrollmentScreen::ShowSigninScreen() {
  actor_->Show();
  actor_->ShowSigninScreen();
}

void EnterpriseEnrollmentScreen::NotifyTestingObservers(bool succeeded) {
  FOR_EACH_OBSERVER(TestingObserver, observers_,
                    OnEnrollmentComplete(succeeded));
}

}  // namespace chromeos
