// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/voice_interaction_value_prop_screen.h"

#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/voice_interaction_value_prop_screen_view.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace {

constexpr const char kUserActionNoThanksPressed[] = "no-thanks-pressed";
constexpr const char kUserActionContinuePressed[] = "continue-pressed";

}  // namespace

VoiceInteractionValuePropScreen::VoiceInteractionValuePropScreen(
    BaseScreenDelegate* base_screen_delegate,
    VoiceInteractionValuePropScreenView* view)
    : BaseScreen(base_screen_delegate,
                 OobeScreen::SCREEN_VOICE_INTERACTION_VALUE_PROP),
      view_(view) {
  DCHECK(view_);
  if (view_)
    view_->Bind(this);
}

VoiceInteractionValuePropScreen::~VoiceInteractionValuePropScreen() {
  if (view_)
    view_->Unbind();
}

void VoiceInteractionValuePropScreen::Show() {
  if (!view_)
    return;

  view_->Show();
}

void VoiceInteractionValuePropScreen::Hide() {
  if (view_)
    view_->Hide();
}

void VoiceInteractionValuePropScreen::OnViewDestroyed(
    VoiceInteractionValuePropScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void VoiceInteractionValuePropScreen::OnUserAction(
    const std::string& action_id) {
  if (action_id == kUserActionNoThanksPressed)
    OnNoThanksPressed();
  else if (action_id == kUserActionContinuePressed)
    OnContinuePressed();
  else
    BaseScreen::OnUserAction(action_id);
}

void VoiceInteractionValuePropScreen::OnNoThanksPressed() {
  Finish(ScreenExitCode::VOICE_INTERACTION_VALUE_PROP_SKIPPED);
}

void VoiceInteractionValuePropScreen::OnContinuePressed() {
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      prefs::kArcVoiceInteractionValuePropAccepted, true);
  Finish(ScreenExitCode::VOICE_INTERACTION_VALUE_PROP_ACCEPTED);
}

}  // namespace chromeos
