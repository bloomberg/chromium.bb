// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/host_pairing_screen.h"

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/enrollment_status_chromeos.h"
#include "components/pairing/host_pairing_controller.h"

namespace chromeos {

using namespace host_pairing;
using namespace pairing_chromeos;

HostPairingScreen::HostPairingScreen(
    BaseScreenDelegate* base_screen_delegate,
    Delegate* delegate,
    HostPairingScreenActor* actor,
    pairing_chromeos::HostPairingController* remora_controller)
    : BaseScreen(base_screen_delegate),
      delegate_(delegate),
      actor_(actor),
      remora_controller_(remora_controller),
      current_stage_(HostPairingController::STAGE_NONE),
      weak_ptr_factory_(this) {
  actor_->SetDelegate(this);
  remora_controller_->AddObserver(this);
}

HostPairingScreen::~HostPairingScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
  remora_controller_->RemoveObserver(this);
}

void HostPairingScreen::CommitContextChanges() {
  if (!context_.HasChanges())
    return;
  base::DictionaryValue diff;
  context_.GetChangesAndReset(&diff);
  if (actor_)
    actor_->OnContextChanged(diff);
}

void HostPairingScreen::PrepareToShow() {
}

void HostPairingScreen::Show() {
  if (actor_)
    actor_->Show();
  PairingStageChanged(remora_controller_->GetCurrentStage());
}

void HostPairingScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string HostPairingScreen::GetName() const {
  return WizardController::kHostPairingScreenName;
}

void HostPairingScreen::PairingStageChanged(Stage new_stage) {
  std::string desired_page;
  switch (new_stage) {
    case HostPairingController::STAGE_WAITING_FOR_CONTROLLER:
    case HostPairingController::STAGE_WAITING_FOR_CONTROLLER_AFTER_UPDATE: {
      desired_page = kPageWelcome;
      break;
    }
    case HostPairingController::STAGE_WAITING_FOR_CODE_CONFIRMATION: {
      desired_page = kPageCodeConfirmation;
      context_.SetString(kContextKeyConfirmationCode,
                         remora_controller_->GetConfirmationCode());
      break;
    }
    case HostPairingController::STAGE_WAITING_FOR_CREDENTIALS:
    case HostPairingController::STAGE_ENROLLING: {
      // TODO(xdai): Use kPageEnrollment for STAGE_ENROLLING. It requires that
      // the Master device sends the domain info over to the Slave device.
      desired_page = kPageEnrollmentIntroduction;
      break;
    }
    case HostPairingController::STAGE_ENROLLMENT_SUCCESS: {
      remora_controller_->RemoveObserver(this);
      Finish(WizardController::ENTERPRISE_ENROLLMENT_COMPLETED);
      break;
    }
    case HostPairingController::STAGE_ENROLLMENT_ERROR: {
      // TODO(xdai): Maybe return to the Network Setup page?
      remora_controller_->RemoveObserver(this);
      desired_page = kPageEnrollmentError;
      break;
    }
    default:
      break;
  }
  current_stage_ = new_stage;
  context_.SetString(kContextKeyDeviceName,
                     remora_controller_->GetDeviceName());
  context_.SetString(kContextKeyPage, desired_page);
  CommitContextChanges();
}

void HostPairingScreen::ConfigureHostRequested(
    bool accepted_eula,
    const std::string& lang,
    const std::string& timezone,
    bool send_reports,
    const std::string& keyboard_layout) {
  VLOG(1) << "ConfigureHostMessage language=" << lang
          << ", timezone=" << timezone
          << ", keyboard_layout=" << keyboard_layout;

  if (delegate_) {
    delegate_->ConfigureHostRequested(
        accepted_eula, lang, timezone, send_reports, keyboard_layout);
  }
}

void HostPairingScreen::AddNetworkRequested(const std::string& onc_spec) {
  if (delegate_)
    delegate_->AddNetworkRequested(onc_spec);
}

void HostPairingScreen::EnrollHostRequested(const std::string& auth_token) {
  policy::EnrollmentConfig enrollment_config =
      g_browser_process->platform_part()
          ->browser_policy_connector_chromeos()
          ->GetPrescribedEnrollmentConfig();
  enrollment_helper_ = EnterpriseEnrollmentHelper::Create(
      this, enrollment_config, std::string());
  enrollment_helper_->EnrollUsingToken(auth_token);
  remora_controller_->OnEnrollmentStatusChanged(
      HostPairingController::ENROLLMENT_STATUS_ENROLLING);
}

void HostPairingScreen::OnActorDestroyed(HostPairingScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

void HostPairingScreen::OnAuthError(const GoogleServiceAuthError& error) {
  OnAnyEnrollmentError();
}

void HostPairingScreen::OnEnrollmentError(policy::EnrollmentStatus status) {
  OnAnyEnrollmentError();
}

void HostPairingScreen::OnOtherError(
    EnterpriseEnrollmentHelper::OtherError error) {
  OnAnyEnrollmentError();
}

void HostPairingScreen::OnDeviceEnrolled(const std::string& additional_token) {
  StartupUtils::MarkDeviceRegistered(base::Bind(&base::DoNothing));
  enrollment_helper_->ClearAuth(base::Bind(&HostPairingScreen::OnAuthCleared,
                                           weak_ptr_factory_.GetWeakPtr()));

  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  const enterprise_management::PolicyData* policy =
      connector->GetDeviceCloudPolicyManager()->core()->store()->policy();

  remora_controller_->SetPermanentId(policy->directory_api_id());
  remora_controller_->OnEnrollmentStatusChanged(
      HostPairingController::ENROLLMENT_STATUS_SUCCESS);
}

void HostPairingScreen::OnDeviceAttributeUploadCompleted(bool success) {}

void HostPairingScreen::OnDeviceAttributeUpdatePermission(bool granted) {}

void HostPairingScreen::OnAuthCleared() {
  enrollment_helper_.reset();
}

void HostPairingScreen::OnAnyEnrollmentError() {
  enrollment_helper_->ClearAuth(base::Bind(&HostPairingScreen::OnAuthCleared,
                                           weak_ptr_factory_.GetWeakPtr()));
  remora_controller_->OnEnrollmentStatusChanged(
      HostPairingController::ENROLLMENT_STATUS_FAILURE);
}

}  // namespace chromeos
