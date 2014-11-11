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
    BaseScreenDelegate* base_screen_delegate,
    Delegate* delegate,
    HostPairingScreenActor* actor,
    pairing_chromeos::HostPairingController* remora_controller)
    : BaseScreen(base_screen_delegate),
      delegate_(delegate),
      actor_(actor),
      remora_controller_(remora_controller),
      current_stage_(HostPairingController::STAGE_NONE) {
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
    default:
      break;
  }
  current_stage_ = new_stage;
  context_.SetString(kContextKeyDeviceName,
                     remora_controller_->GetDeviceName());
  context_.SetString(kContextKeyPage, desired_page);
  CommitContextChanges();
}

void HostPairingScreen::ConfigureHost(bool accepted_eula,
                                      const std::string& lang,
                                      const std::string& timezone,
                                      bool send_reports,
                                      const std::string& keyboard_layout) {
  VLOG(1) << "ConfigureHostMessage language=" << lang
          << ", timezone=" << timezone
          << ", keyboard_layout=" << keyboard_layout;

  remora_controller_->RemoveObserver(this);
  if (delegate_) {
    delegate_->ConfigureHost(
        accepted_eula, lang, timezone, send_reports, keyboard_layout);
  }
  Finish(WizardController::HOST_PAIRING_FINISHED);
}

void HostPairingScreen::EnrollHost(const std::string& auth_token) {
  NOTREACHED();
}

void HostPairingScreen::OnActorDestroyed(HostPairingScreenActor* actor) {
  if (actor_ == actor)
    actor_ = NULL;
}

}  // namespace chromeos
