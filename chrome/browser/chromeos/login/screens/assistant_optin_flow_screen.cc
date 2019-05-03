// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/assistant_optin_flow_screen.h"

#include "chrome/browser/chromeos/assistant/assistant_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/assistant_optin_flow_screen_handler.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/constants/chromeos_switches.h"

namespace chromeos {
namespace {

constexpr const char kFlowFinished[] = "flow-finished";

}  // namespace

AssistantOptInFlowScreen::AssistantOptInFlowScreen(
    AssistantOptInFlowScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(AssistantOptInFlowScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

AssistantOptInFlowScreen::~AssistantOptInFlowScreen() {
  if (view_)
    view_->Unbind();
}

void AssistantOptInFlowScreen::Show() {
  if (!view_)
    return;

#if BUILDFLAG(ENABLE_CROS_ASSISTANT)
  if (chromeos::switches::IsAssistantEnabled() &&
      ::assistant::IsAssistantAllowedForProfile(
          ProfileManager::GetActiveUserProfile()) ==
          ash::mojom::AssistantAllowedState::ALLOWED) {
    view_->Show();
    return;
  }
#endif
  exit_callback_.Run();
}

void AssistantOptInFlowScreen::Hide() {
  if (view_)
    view_->Hide();
}

void AssistantOptInFlowScreen::OnViewDestroyed(
    AssistantOptInFlowScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void AssistantOptInFlowScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kFlowFinished)
    exit_callback_.Run();
  else
    BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
