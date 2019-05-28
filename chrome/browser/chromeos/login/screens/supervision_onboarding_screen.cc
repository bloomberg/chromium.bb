// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/supervision_onboarding_screen.h"

#include "base/logging.h"
#include "chrome/browser/ui/webui/chromeos/login/supervision_onboarding_screen_handler.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"

namespace chromeos {
namespace {

constexpr const char kFinishedUserAction[] = "setup-finished";

}  // namespace

SupervisionOnboardingScreen::SupervisionOnboardingScreen(
    SupervisionOnboardingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(SupervisionOnboardingScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  if (view_)
    view_->Bind(this);
}

SupervisionOnboardingScreen::~SupervisionOnboardingScreen() {
  if (view_)
    view_->Unbind();
}

void SupervisionOnboardingScreen::Show() {
#if defined(KIOSK_NEXT)
  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  DCHECK(user_manager->IsUserLoggedIn());

  if (view_ &&
      base::FeatureList::IsEnabled(
          features::kSupervisionOnboardingEligibility) &&
      user_manager->IsLoggedInAsChildUser()) {
    view_->Show();
    return;
  }
#endif

  Exit();
}

void SupervisionOnboardingScreen::Hide() {
  if (view_)
    view_->Hide();
}

void SupervisionOnboardingScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kFinishedUserAction) {
    Exit();
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

void SupervisionOnboardingScreen::OnViewDestroyed(
    SupervisionOnboardingScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void SupervisionOnboardingScreen::Exit() {
  exit_callback_.Run();
}

}  // namespace chromeos
