// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/supervision_onboarding_screen_handler.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/screens/supervision_onboarding_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "components/login/localized_values_builder.h"

namespace chromeos {

constexpr StaticOobeScreenId SupervisionOnboardingScreenView::kScreenId;

SupervisionOnboardingScreenHandler::SupervisionOnboardingScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container) {
  set_user_acted_method_path("login.SupervisionOnboardingScreen.userActed");
  supervision_onboarding_controller_ =
      std::make_unique<supervision::OnboardingControllerImpl>();
}

SupervisionOnboardingScreenHandler::~SupervisionOnboardingScreenHandler() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void SupervisionOnboardingScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  // TODO(ltenorio): Add the strings for the back/next buttons here.
}

void SupervisionOnboardingScreenHandler::Bind(
    SupervisionOnboardingScreen* screen) {
  BaseScreenHandler::SetBaseScreen(screen);
  screen_ = screen;
}

void SupervisionOnboardingScreenHandler::Unbind() {
  screen_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

void SupervisionOnboardingScreenHandler::Show() {
  ShowScreen(kScreenId);

  GetOobeUI()->AddHandlerToRegistry(base::BindRepeating(
      &SupervisionOnboardingScreenHandler::BindSupervisionOnboardingController,
      base::Unretained(this)));

  CallJS("login.SupervisionOnboardingScreen.setupMojo");
}

void SupervisionOnboardingScreenHandler::Hide() {}

void SupervisionOnboardingScreenHandler::Initialize() {}

void SupervisionOnboardingScreenHandler::BindSupervisionOnboardingController(
    supervision::mojom::OnboardingControllerRequest request) {
  supervision_onboarding_controller_->BindRequest(std::move(request));
}

}  // namespace chromeos
