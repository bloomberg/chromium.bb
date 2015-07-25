// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/enrollment/enrollment_screen.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_uma.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/pairing/controller_pairing_controller.h"
#include "google_apis/gaia/gaia_auth_util.h"

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

namespace {

const char * const kMetricEnrollmentTimeCancel =
    "Enterprise.EnrollmentTime.Cancel";
const char * const kMetricEnrollmentTimeFailure =
    "Enterprise.EnrollmentTime.Failure";
const char * const kMetricEnrollmentTimeSuccess =
    "Enterprise.EnrollmentTime.Success";

}  // namespace

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
      enrollment_failed_once_(false),
      weak_ptr_factory_(this) {
  // Init the TPM if it has not been done until now (in debug build we might
  // have not done that yet).
  DBusThreadManager::Get()->GetCryptohomeClient()->TpmCanAttemptOwnership(
      EmptyVoidDBusMethodCallback());
}

EnrollmentScreen::~EnrollmentScreen() {
  if (remora_controller_)
    remora_controller_->RemoveObserver(this);
  DCHECK(!enrollment_helper_ || g_browser_process->IsShuttingDown());
}

void EnrollmentScreen::SetParameters(
    const policy::EnrollmentConfig& enrollment_config,
    pairing_chromeos::ControllerPairingController* shark_controller,
    pairing_chromeos::HostPairingController* remora_controller) {
  enrollment_config_ = enrollment_config;
  shark_controller_ = shark_controller;
  if (remora_controller_)
    remora_controller_->RemoveObserver(this);
  remora_controller_ = remora_controller;
  if (remora_controller_)
    remora_controller_->AddObserver(this);
  actor_->SetParameters(this, enrollment_config_);
}

void EnrollmentScreen::CreateEnrollmentHelper() {
  DCHECK(!enrollment_helper_);
  enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
      this, enrollment_config_, enrolling_user_domain_);
}

void EnrollmentScreen::ClearAuth(const base::Closure& callback) {
  if (!enrollment_helper_) {
    callback.Run();
    return;
  }
  enrollment_helper_->ClearAuth(base::Bind(&EnrollmentScreen::OnAuthCleared,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           callback));
}

void EnrollmentScreen::OnAuthCleared(const base::Closure& callback) {
  enrollment_helper_.reset();
  callback.Run();
}

void EnrollmentScreen::PrepareToShow() {
  actor_->PrepareToShow();
}

void EnrollmentScreen::Show() {
  UMA(policy::kMetricEnrollmentTriggered);
  ClearAuth(base::Bind(&EnrollmentScreen::ShowSigninScreen,
                       weak_ptr_factory_.GetWeakPtr()));
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

void EnrollmentScreen::EnrollHostRequested(const std::string& auth_token) {
  actor_->Show();
  actor_->ShowEnrollmentSpinnerScreen();
  CreateEnrollmentHelper();
  enrollment_helper_->EnrollUsingToken(auth_token);
  if (remora_controller_) {
    remora_controller_->OnEnrollmentStatusChanged(
        HostPairingController::ENROLLMENT_STATUS_ENROLLING);
  }
}

void EnrollmentScreen::OnLoginDone(const std::string& user,
                                   const std::string& auth_code) {
  LOG_IF(ERROR, auth_code.empty()) << "Auth code is empty.";
  elapsed_timer_.reset(new base::ElapsedTimer());
  enrolling_user_domain_ = gaia::ExtractDomainName(user);

  UMA(enrollment_failed_once_ ? policy::kMetricEnrollmentRestarted
                              : policy::kMetricEnrollmentStarted);

  actor_->ShowEnrollmentSpinnerScreen();
  CreateEnrollmentHelper();
  enrollment_helper_->EnrollUsingAuthCode(
      auth_code, shark_controller_ != NULL /* fetch_additional_token */);
}

void EnrollmentScreen::OnRetry() {
  ClearAuth(base::Bind(&EnrollmentScreen::ShowSigninScreen,
                       weak_ptr_factory_.GetWeakPtr()));
}

void EnrollmentScreen::OnCancel() {
  UMA(policy::kMetricEnrollmentCancelled);
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeCancel, elapsed_timer_);

  const BaseScreenDelegate::ExitCodes exit_code =
      enrollment_config_.is_forced()
          ? BaseScreenDelegate::ENTERPRISE_ENROLLMENT_BACK
          : BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED;
  ClearAuth(
      base::Bind(&EnrollmentScreen::Finish, base::Unretained(this), exit_code));
}

void EnrollmentScreen::OnConfirmationClosed() {
  ClearAuth(base::Bind(&EnrollmentScreen::Finish, base::Unretained(this),
                       BaseScreenDelegate::ENTERPRISE_ENROLLMENT_COMPLETED));
}

void EnrollmentScreen::OnAuthError(const GoogleServiceAuthError& error) {
  DCHECK(!remora_controller_);
  OnAnyEnrollmentError();
  actor_->ShowAuthError(error);
}

void EnrollmentScreen::OnEnrollmentError(policy::EnrollmentStatus status) {
  OnAnyEnrollmentError();
  actor_->ShowEnrollmentStatus(status);
}

void EnrollmentScreen::OnOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  OnAnyEnrollmentError();
  actor_->ShowOtherError(error);
}

void EnrollmentScreen::OnDeviceEnrolled(const std::string& additional_token) {
  if (!additional_token.empty())
    SendEnrollmentAuthToken(additional_token);

  enrollment_helper_->GetDeviceAttributeUpdatePermission();
}

void EnrollmentScreen::OnDeviceAttributeProvided(const std::string& asset_id,
                                                 const std::string& location) {
  enrollment_helper_->UpdateDeviceAttributes(asset_id, location);
}

void EnrollmentScreen::OnDeviceAttributeUpdatePermission(bool granted) {
  // If user is permitted to update device attributes
  // Show attribute prompt screen
  if (granted) {
    StartupUtils::MarkDeviceRegistered(
        base::Bind(&EnrollmentScreen::ShowAttributePromptScreen,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    StartupUtils::MarkDeviceRegistered(
        base::Bind(&EnrollmentScreen::ShowEnrollmentStatusOnSuccess,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  if (remora_controller_) {
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    const enterprise_management::PolicyData* policy =
        connector->GetDeviceCloudPolicyManager()->core()->store()->policy();

    remora_controller_->SetPermanentId(policy->directory_api_id());
    remora_controller_->OnEnrollmentStatusChanged(
        HostPairingController::ENROLLMENT_STATUS_SUCCESS);
  }
}

void EnrollmentScreen::OnDeviceAttributeUploadCompleted(bool success) {
  if (success) {
    // If the device attributes have been successfully uploaded, fetch policy.
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->GetDeviceCloudPolicyManager()->core()->RefreshSoon();
    actor_->ShowEnrollmentStatus(policy::EnrollmentStatus::ForStatus(
        policy::EnrollmentStatus::STATUS_SUCCESS));
  } else {
    actor_->ShowEnrollmentStatus(policy::EnrollmentStatus::ForStatus(
        policy::EnrollmentStatus::STATUS_ATTRIBUTE_UPDATE_FAILED));
  }
}

void EnrollmentScreen::ShowAttributePromptScreen() {
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  policy::DeviceCloudPolicyManagerChromeOS* policy_manager =
      connector->GetDeviceCloudPolicyManager();

  policy::CloudPolicyStore* store = policy_manager->core()->store();

  const enterprise_management::PolicyData* policy = store->policy();

  std::string asset_id = policy ? policy->annotated_asset_id() : std::string();
  std::string location = policy ? policy->annotated_location() : std::string();
  actor_->ShowAttributePromptScreen(asset_id, location);
}

void EnrollmentScreen::SendEnrollmentAuthToken(const std::string& token) {
  // TODO(achuith, zork): Extract and send domain.
  DCHECK(shark_controller_);
  shark_controller_->OnAuthenticationDone("", token);
}

void EnrollmentScreen::ShowEnrollmentStatusOnSuccess() {
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeSuccess, elapsed_timer_);
  actor_->ShowEnrollmentStatus(policy::EnrollmentStatus::ForStatus(
      policy::EnrollmentStatus::STATUS_SUCCESS));
}

void EnrollmentScreen::UMA(policy::MetricEnrollment sample) {
  EnrollmentUMA(sample, enrollment_config_.mode);
}

void EnrollmentScreen::ShowSigninScreen() {
  actor_->Show();
  actor_->ShowSigninScreen();
}

void EnrollmentScreen::OnAnyEnrollmentError() {
  enrollment_failed_once_ = true;
  if (elapsed_timer_)
    UMA_ENROLLMENT_TIME(kMetricEnrollmentTimeFailure, elapsed_timer_);
  if (remora_controller_) {
    remora_controller_->OnEnrollmentStatusChanged(
        HostPairingController::ENROLLMENT_STATUS_FAILURE);
  }
}

}  // namespace chromeos
