// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/host_pairing_screen.h"

#include "base/command_line.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chromeos/chromeos_switches.h"
#include "components/pairing/fake_host_pairing_controller.h"

namespace chromeos {

using namespace host_pairing;
using namespace pairing_chromeos;

HostPairingScreen::HostPairingScreen(ScreenObserver* observer,
                                     HostPairingScreenActor* actor)
    : WizardScreen(observer),
      actor_(actor),
      current_stage_(HostPairingController::STAGE_NONE) {
  actor_->SetDelegate(this);
  std::string controller_config =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kShowHostPairingDemo);
  controller_.reset(new FakeHostPairingController(controller_config));
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
  controller_->StartPairing();
}

void HostPairingScreen::Hide() {
  if (actor_)
    actor_->Hide();
}

std::string HostPairingScreen::GetName() const {
  return WizardController::kHostPairingScreenName;
}

void HostPairingScreen::PairingStageChanged(Stage new_stage) {
  DCHECK(new_stage != current_stage_);

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
      get_screen_observer()->OnExit(WizardController::HOST_PAIRING_FINISHED);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
  current_stage_ = new_stage;
  context_.SetString(kContextKeyDeviceName, controller_->GetDeviceName());
  context_.SetString(kContextKeyPage, desired_page);
  CommitContextChanges();
}

void HostPairingScreen::UpdateAdvanced(const UpdateProgress& progress) {
  context_.SetDouble(kContextKeyUpdateProgress, progress.progress);
  CommitContextChanges();
}

void HostPairingScreen::OnActorDestroyed(HostPairingScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
