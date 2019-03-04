// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/assistant_setup_controller.h"

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/util/deep_link_util.h"
#include "ash/assistant/util/i18n_util.h"
#include "ash/public/interfaces/voice_interaction_controller.mojom.h"
#include "ash/shell.h"
#include "ash/voice_interaction/voice_interaction_controller.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromeos/services/assistant/public/features.h"

namespace {

constexpr char kGSuiteAdministratorInstructionsUrl[] =
    "https://support.google.com/a/answer/6304876";

}  // namespace

namespace ash {

AssistantSetupController::AssistantSetupController(
    AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller), binding_(this) {
  assistant_controller_->AddObserver(this);
}

AssistantSetupController::~AssistantSetupController() {
  assistant_controller_->RemoveObserver(this);
}

void AssistantSetupController::BindRequest(
    mojom::AssistantSetupControllerRequest request) {
  binding_.Bind(std::move(request));
}

void AssistantSetupController::SetAssistantSetup(
    mojom::AssistantSetupPtr assistant_setup) {
  assistant_setup_ = std::move(assistant_setup);
}

void AssistantSetupController::OnAssistantControllerConstructed() {
  assistant_controller_->view_delegate()->AddObserver(this);
}

void AssistantSetupController::OnAssistantControllerDestroying() {
  assistant_controller_->view_delegate()->RemoveObserver(this);
}

void AssistantSetupController::OnDeepLinkReceived(
    assistant::util::DeepLinkType type,
    const std::map<std::string, std::string>& params) {
  using namespace assistant::util;

  if (type != DeepLinkType::kOnboarding)
    return;

  base::Optional<bool> relaunch =
      GetDeepLinkParamAsBool(params, DeepLinkParam::kRelaunch);

  StartOnboarding(relaunch.value_or(false));
}

void AssistantSetupController::OnOptInButtonPressed() {
  mojom::ConsentStatus consent_status =
      Shell::Get()->voice_interaction_controller()->consent_status().value_or(
          mojom::ConsentStatus::kUnknown);

  if (consent_status == mojom::ConsentStatus::kUnauthorized) {
    assistant_controller_->OpenUrl(assistant::util::CreateLocalizedGURL(
        kGSuiteAdministratorInstructionsUrl));
  } else {
    StartOnboarding(/*relaunch=*/true);
  }
}

void AssistantSetupController::StartOnboarding(bool relaunch,
                                               mojom::FlowType type) {
  if (!assistant_setup_)
    return;

  if (relaunch) {
    assistant_setup_->StartAssistantOptInFlow(
        type,
        base::BindOnce(
            [](AssistantController* assistant_controller, bool completed) {
              if (completed) {
                assistant_controller->ui_controller()->ShowUi(
                    AssistantEntryPoint::kSetup);
              }
            },
            // AssistantController owns |assistant_setup_| so a raw pointer is
            // safe.
            assistant_controller_));
  } else {
    assistant_setup_->StartAssistantOptInFlow(type, base::DoNothing());
  }

  // Assistant UI should be hidden while the user onboards.
  assistant_controller_->ui_controller()->HideUi(AssistantExitPoint::kSetup);
}

}  // namespace ash
