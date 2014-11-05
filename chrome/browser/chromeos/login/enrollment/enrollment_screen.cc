// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/login_utils.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/auto_enrollment_client.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/pairing/controller_pairing_controller.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "policy/proto/device_management_backend.pb.h"

using namespace pairing_chromeos;

// Do not change the UMA histogram parameters without renaming the histograms!
#define UMA_ENROLLMENT_TIME(histogram_name, elapsed_timer) \
  do {                                                     \
    UMA_HISTOGRAM_CUSTOM_TIMES(                            \
      (histogram_name),                                    \
      (elapsed_timer)->Elapsed(),                          \
      base::TimeDelta::FromMilliseconds(100) /* min */,    \
      base::TimeDelta::FromMinutes(15) /* max */,          \
      100 /* bucket_count */);                             \
  } while (0)

namespace chromeos {

// static
EnrollmentScreen* EnrollmentScreen::Get(ScreenManager* manager) {
  return static_cast<EnrollmentScreen*>(
      manager->GetScreen(WizardController::kEnrollmentScreenName));
}

EnrollmentScreen::EnrollmentScreen(BaseScreenDelegate* base_screen_delegate,
                                   EnrollmentScreenActor* actor)
    : BaseScreen(base_screen_delegate),
      shark_controller_(NULL),
      remora_controller_(NULL),
      actor_(actor),
      enrollment_mode_(EnrollmentScreenActor::ENROLLMENT_MODE_MANUAL),
      enrollment_failed_once_(false),
      remora_token_sent_(false),
      weak_ptr_factory_(this) {
  // Init the TPM if it has not been done until now (in debug build we might
  // have not done that yet).
  DBusThreadManager::Get()->GetCryptohomeClient()->TpmCanAttemptOwnership(
      EmptyVoidDBusMethodCallback());
}

EnrollmentScreen::~EnrollmentScreen() {
  if (remora_controller_)
    remora_controller_->RemoveObserver(this);
}

void EnrollmentScreen::SetParameters(
    EnrollmentScreenActor::EnrollmentMode enrollment_mode,
    const std::string& management_domain,
    const std::string& user,
    pairing_chromeos::ControllerPairingController* shark_controller,
    pairing_chromeos::HostPairingController* remora_controller) {
  enrollment_mode_ = enrollment_mode;
  user_ = user.empty() ? user : gaia::CanonicalizeEmail(user);
  shark_controller_ = shark_controller;
  if (remora_controller_)
    remora_controller_->RemoveObserver(this);
  remora_controller_ = remora_controller;
  if (remora_controller_)
    remora_controller_->AddObserver(this);
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

void EnrollmentScreen::PairingStageChanged(Stage new_stage) {
  DCHECK(remora_controller_);
  if (new_stage == HostPairingController::STAGE_FINISHED) {
    remora_controller_->RemoveObserver(this);
    remora_controller_ = NULL;
    OnConfirmationClosed();
  }
}

void EnrollmentScreen::ConfigureHost(bool accepted_eula,
                                     const std::string& lang,
                                     const std::string& timezone,
                                     bool send_reports,
                                     const std::string& keyboard_layout) {
}

void EnrollmentScreen::EnrollHost(const std::string& auth_token) {
  actor_->Show();
  actor_->ShowEnrollmentSpinnerScreen();
  OnOAuthTokenAvailable(auth_token);
  if (remora_controller_) {
    remora_controller_->OnEnrollmentStatusChanged(
        HostPairingController::ENROLLMENT_STATUS_ENROLLING);
  }
}

void EnrollmentScreen::OnLoginDone(const std::string& user) {
  elapsed_timer_.reset(new base::ElapsedTimer());
  user_ = gaia::CanonicalizeEmail(user);

  if (is_auto_enrollment())
    UMA(policy::kMetricEnrollmentAutoRestarted);
  else if (enrollment_failed_once_)
    UMA(policy::kMetricEnrollmentRestarted);
  else
    UMA(policy::kMetricEnrollmentStarted);

  actor_->ShowEnrollmentSpinnerScreen();
  actor_->FetchOAuthToken();
}

void EnrollmentScreen::OnAuthError(const GoogleServiceAuthError& error) {
  switch (error.state()) {
    case GoogleServiceAuthError::NONE:
    case GoogleServiceAuthError::CAPTCHA_REQUIRED:
    case GoogleServiceAuthError::TWO_FACTOR:
    case GoogleServiceAuthError::HOSTED_NOT_ALLOWED:
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::REQUEST_CANCELED:
    case GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::WEB_LOGIN_REQUIRED:
      UMAFailure(policy::kMetricEnrollmentLoginFailed);
      LOG(ERROR) << "Auth error " << error.state();
      break;
    case GoogleServiceAuthError::USER_NOT_SIGNED_UP:
      UMAFailure(policy::kMetricEnrollmentAccountNotSignedUp);
      LOG(ERROR) << "Account not signed up " << error.state();
      break;
    case GoogleServiceAuthError::ACCOUNT_DELETED:
      UMAFailure(policy::kMetricEnrollmentAccountDeleted);
      LOG(ERROR) << "Account deleted " << error.state();
      break;
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      UMAFailure(policy::kMetricEnrollmentAccountDisabled);
      LOG(ERROR) << "Account disabled " << error.state();
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      UMAFailure(policy::kMetricEnrollmentNetworkFailed);
      LOG(WARNING) << "Network error " << error.state();
      break;
    case GoogleServiceAuthError::NUM_STATES:
      NOTREACHED();
      break;
  }

  enrollment_failed_once_ = true;
  actor_->ShowAuthError(error);
}

void EnrollmentScreen::OnOAuthTokenAvailable(const std::string& token) {
  VLOG(1) << "OnOAuthTokenAvailable " << token;
  const bool is_shark =
      g_browser_process->platform_part()->browser_policy_connector_chromeos()->
          GetDeviceCloudPolicyManager()->IsSharkRequisition();

  if (is_shark && !remora_token_sent_) {
    // Fetch a second token for shark devices.
    remora_token_sent_ = true;
    SendEnrollmentAuthToken(token);
    actor_->FetchOAuthToken();
  } else {
    RegisterForDevicePolicy(token);
  }
}

void EnrollmentScreen::OnRetry() {
  actor_->ResetAuth(base::Bind(&EnrollmentScreen::ShowSigninScreen,
                               weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::OnCancel() {
  UMA(is_auto_enrollment() ? policy::kMetricEnrollmentAutoCancelled
                           : policy::kMetricEnrollmentCancelled);
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME("Enterprise.EnrollmentTime.Cancel", elapsed_timer_);
  if (enrollment_mode_ == EnrollmentScreenActor::ENROLLMENT_MODE_FORCED ||
      enrollment_mode_ == EnrollmentScreenActor::ENROLLMENT_MODE_RECOVERY) {
    actor_->ResetAuth(
        base::Bind(&BaseScreenDelegate::OnExit,
                   base::Unretained(get_base_screen_delegate()),
                   BaseScreenDelegate::ENTERPRISE_ENROLLMENT_BACK));
    return;
  }

  if (is_auto_enrollment())
    policy::AutoEnrollmentClient::CancelAutoEnrollment();
  actor_->ResetAuth(
      base::Bind(&BaseScreenDelegate::OnExit,
                 base::Unretained(get_base_screen_delegate()),
                 BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED));
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
    get_base_screen_delegate()->OnExit(
        BaseScreenDelegate::ENTERPRISE_AUTO_MAGIC_ENROLLMENT_COMPLETED);
  } else {
    actor_->ResetAuth(
        base::Bind(&BaseScreenDelegate::OnExit,
                   base::Unretained(get_base_screen_delegate()),
                   BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED));
  }
}

void EnrollmentScreen::RegisterForDevicePolicy(const std::string& token) {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->IsEnterpriseManaged() &&
      connector->GetEnterpriseDomain() != gaia::ExtractDomainName(user_)) {
    LOG(ERROR) << "Trying to re-enroll to a different domain than "
               << connector->GetEnterpriseDomain();
    UMAFailure(policy::kMetricEnrollmentPrecheckDomainMismatch);
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

void EnrollmentScreen::SendEnrollmentAuthToken(const std::string& token) {
  // TODO(achuith, zork): Extract and send domain.
  if (shark_controller_)
    shark_controller_->OnAuthenticationDone("", token);
}

void EnrollmentScreen::ShowEnrollmentStatusOnSuccess(
    const policy::EnrollmentStatus& status) {
  StartupUtils::MarkOobeCompleted();
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME("Enterprise.EnrollmentTime.Success", elapsed_timer_);
  actor_->ShowEnrollmentStatus(status);
}

void EnrollmentScreen::ReportEnrollmentStatus(policy::EnrollmentStatus status) {
  switch (status.status()) {
    case policy::EnrollmentStatus::STATUS_SUCCESS:
      StartupUtils::MarkDeviceRegistered(
          base::Bind(&EnrollmentScreen::ShowEnrollmentStatusOnSuccess,
                     weak_ptr_factory_.GetWeakPtr(),
                     status));
      UMA(is_auto_enrollment() ? policy::kMetricEnrollmentAutoOK
                               : policy::kMetricEnrollmentOK);
      if (remora_controller_) {
        remora_controller_->OnEnrollmentStatusChanged(
            HostPairingController::ENROLLMENT_STATUS_SUCCESS);
      }
      return;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_FAILED:
    case policy::EnrollmentStatus::STATUS_POLICY_FETCH_FAILED:
      switch (status.client_status()) {
        case policy::DM_STATUS_SUCCESS:
          NOTREACHED();
          break;
        case policy::DM_STATUS_REQUEST_INVALID:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyPayloadInvalid);
          break;
        case policy::DM_STATUS_SERVICE_DEVICE_NOT_FOUND:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyDeviceNotFound);
          break;
        case policy::DM_STATUS_SERVICE_MANAGEMENT_TOKEN_INVALID:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyDMTokenInvalid);
          break;
        case policy::DM_STATUS_SERVICE_ACTIVATION_PENDING:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyActivationPending);
          break;
        case policy::DM_STATUS_SERVICE_DEVICE_ID_CONFLICT:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyDeviceIdConflict);
          break;
        case policy::DM_STATUS_SERVICE_POLICY_NOT_FOUND:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyNotFound);
          break;
        case policy::DM_STATUS_REQUEST_FAILED:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyRequestFailed);
          break;
        case policy::DM_STATUS_TEMPORARY_UNAVAILABLE:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyTempUnavailable);
          break;
        case policy::DM_STATUS_HTTP_STATUS_ERROR:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyHttpError);
          break;
        case policy::DM_STATUS_RESPONSE_DECODING_ERROR:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyResponseInvalid);
          break;
        case policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED:
          UMAFailure(policy::kMetricEnrollmentNotSupported);
          break;
        case policy::DM_STATUS_SERVICE_INVALID_SERIAL_NUMBER:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyInvalidSerial);
          break;
        case policy::DM_STATUS_SERVICE_MISSING_LICENSES:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyMissingLicenses);
          break;
        case policy::DM_STATUS_SERVICE_DEPROVISIONED:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyDeprovisioned);
          break;
        case policy::DM_STATUS_SERVICE_DOMAIN_MISMATCH:
          UMAFailure(policy::kMetricEnrollmentRegisterPolicyDomainMismatch);
          break;
      }
      break;
    case policy::EnrollmentStatus::STATUS_REGISTRATION_BAD_MODE:
      UMAFailure(policy::kMetricEnrollmentInvalidEnrollmentMode);
      break;
    case policy::EnrollmentStatus::STATUS_NO_STATE_KEYS:
      UMAFailure(policy::kMetricEnrollmentNoStateKeys);
      break;
    case policy::EnrollmentStatus::STATUS_VALIDATION_FAILED:
      UMAFailure(policy::kMetricEnrollmentPolicyValidationFailed);
      break;
    case policy::EnrollmentStatus::STATUS_STORE_ERROR:
      UMAFailure(policy::kMetricEnrollmentCloudPolicyStoreError);
      break;
    case policy::EnrollmentStatus::STATUS_LOCK_ERROR:
      switch (status.lock_status()) {
        case policy::EnterpriseInstallAttributes::LOCK_SUCCESS:
        case policy::EnterpriseInstallAttributes::LOCK_NOT_READY:
          NOTREACHED();
          break;
        case policy::EnterpriseInstallAttributes::LOCK_TIMEOUT:
          UMAFailure(policy::kMetricEnrollmentLockboxTimeoutError);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_BACKEND_INVALID:
          UMAFailure(policy::kMetricEnrollmentLockBackendInvalid);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_ALREADY_LOCKED:
          UMAFailure(policy::kMetricEnrollmentLockAlreadyLocked);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_SET_ERROR:
          UMAFailure(policy::kMetricEnrollmentLockSetError);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_FINALIZE_ERROR:
          UMAFailure(policy::kMetricEnrollmentLockFinalizeError);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_READBACK_ERROR:
          UMAFailure(policy::kMetricEnrollmentLockReadbackError);
          break;
        case policy::EnterpriseInstallAttributes::LOCK_WRONG_DOMAIN:
          UMAFailure(policy::kMetricEnrollmentLockDomainMismatch);
          break;
      }
      break;
    case policy::EnrollmentStatus::STATUS_ROBOT_AUTH_FETCH_FAILED:
      UMAFailure(policy::kMetricEnrollmentRobotAuthCodeFetchFailed);
      break;
    case policy::EnrollmentStatus::STATUS_ROBOT_REFRESH_FETCH_FAILED:
      UMAFailure(policy::kMetricEnrollmentRobotRefreshTokenFetchFailed);
      break;
    case policy::EnrollmentStatus::STATUS_ROBOT_REFRESH_STORE_FAILED:
      UMAFailure(policy::kMetricEnrollmentRobotRefreshTokenStoreFailed);
      break;
    case policy::EnrollmentStatus::STATUS_STORE_TOKEN_AND_ID_FAILED:
      // This error should not happen for enterprise enrollment, it only affects
      // consumer enrollment.
      UMAFailure(policy::kMetricEnrollmentStoreTokenAndIdFailed);
      NOTREACHED();
      break;
  }

  if (remora_controller_) {
    remora_controller_->OnEnrollmentStatusChanged(
        HostPairingController::ENROLLMENT_STATUS_FAILURE);
  }
  enrollment_failed_once_ = true;
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME("Enterprise.EnrollmentTime.Failure", elapsed_timer_);
  actor_->ShowEnrollmentStatus(status);
}

void EnrollmentScreen::UMA(policy::MetricEnrollment sample) {
  switch (enrollment_mode_) {
    case EnrollmentScreenActor::ENROLLMENT_MODE_MANUAL:
    case EnrollmentScreenActor::ENROLLMENT_MODE_AUTO:
      UMA_HISTOGRAM_SPARSE_SLOWLY("Enterprise.Enrollment", sample);
      break;
    case EnrollmentScreenActor::ENROLLMENT_MODE_FORCED:
      UMA_HISTOGRAM_SPARSE_SLOWLY("Enterprise.EnrollmentForced", sample);
      break;
    case EnrollmentScreenActor::ENROLLMENT_MODE_RECOVERY:
      UMA_HISTOGRAM_SPARSE_SLOWLY("Enterprise.EnrollmentRecovery", sample);
      break;
    case EnrollmentScreenActor::ENROLLMENT_MODE_COUNT:
      NOTREACHED();
      break;
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
