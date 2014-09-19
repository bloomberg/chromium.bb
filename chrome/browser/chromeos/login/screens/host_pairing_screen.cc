// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/host_pairing_screen.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "components/pairing/host_pairing_controller.h"

namespace chromeos {

using namespace host_pairing;
using namespace pairing_chromeos;

HostPairingScreen::HostPairingScreen(
    ScreenObserver* observer,
    HostPairingScreenActor* actor,
    pairing_chromeos::HostPairingController* controller)
    : WizardScreen(observer),
      actor_(actor),
      controller_(controller),
      current_stage_(HostPairingController::STAGE_NONE) {
  actor_->SetDelegate(this);
  controller_->AddObserver(this);
}

HostPairingScreen::~HostPairingScreen() {
  if (actor_)
    actor_->SetDelegate(NULL);
  controller_->RemoveObserver(this);
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
  PairingStageChanged(controller_->GetCurrentStage());
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
    case HostPairingController::STAGE_NONE:
    case HostPairingController::STAGE_INITIALIZATION_ERROR: {
      break;
    }
    case HostPairingController::STAGE_WAITING_FOR_CONTROLLER:
    case HostPairingController::STAGE_WAITING_FOR_CONTROLLER_AFTER_UPDATE: {
      desired_page = kPageWelcome;
      break;
    }
    case HostPairingController::STAGE_WAITING_FOR_CODE_CONFIRMATION: {
      desired_page = kPageCodeConfirmation;
      context_.SetString(kContextKeyConfirmationCode,
                         controller_->GetConfirmationCode());
      break;
    }
    case HostPairingController::STAGE_UPDATING: {
      desired_page = kPageUpdate;
      context_.SetDouble(kContextKeyUpdateProgress, 0.0);
      break;
    }
    case HostPairingController::STAGE_WAITING_FOR_CREDENTIALS: {
      desired_page = kPageEnrollmentIntroduction;
      break;
    }
    case HostPairingController::STAGE_ENROLLING: {
      desired_page = kPageEnrollment;
      context_.SetString(kContextKeyEnrollmentDomain,
                         controller_->GetEnrollmentDomain());
      break;
    }
    case HostPairingController::STAGE_ENROLLMENT_ERROR: {
      desired_page = kPageEnrollmentError;
      break;
    }
    case HostPairingController::STAGE_PAIRING_DONE: {
      desired_page = kPagePairingDone;
      break;
    }
    case HostPairingController::STAGE_FINISHED: {
      // This page is closed in EnrollHost.
      break;
    }
  }
  current_stage_ = new_stage;
  context_.SetString(kContextKeyDeviceName, controller_->GetDeviceName());
  context_.SetString(kContextKeyPage, desired_page);
  CommitContextChanges();
}

void HostPairingScreen::ConfigureHost(bool accepted_eula,
                                      const std::string& lang,
                                      const std::string& timezone,
                                      bool send_reports,
                                      const std::string& keyboard_layout) {
  // TODO(zork): Get configuration from UI and send to Host.
  // (http://crbug.com/405744)
}

void HostPairingScreen::EnrollHost(const std::string& auth_token) {
  controller_->RemoveObserver(this);
  WizardController::default_controller()->OnEnrollmentAuthTokenReceived(
      auth_token);
}

void HostPairingScreen::OnActorDestroyed(HostPairingScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
