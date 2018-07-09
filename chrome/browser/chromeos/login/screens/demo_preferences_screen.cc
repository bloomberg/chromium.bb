// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/demo_preferences_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/demo_preferences_screen_view.h"
#include "chrome/browser/chromeos/login/screens/screen_exit_code.h"

namespace chromeos {

constexpr char kUserActionContinue[] = "continue-setup";
constexpr char kUserActionClose[] = "close-setup";

DemoPreferencesScreen::DemoPreferencesScreen(
    BaseScreenDelegate* base_screen_delegate,
    DemoPreferencesScreenView* view)
    : BaseScreen(base_screen_delegate,
                 OobeScreen::SCREEN_OOBE_DEMO_PREFERENCES),
      view_(view) {
  DCHECK(view_);
  view_->Bind(this);
}

DemoPreferencesScreen::~DemoPreferencesScreen() {
  if (view_)
    view_->Bind(nullptr);
}

void DemoPreferencesScreen::Show() {
  if (view_)
    view_->Show();
}

void DemoPreferencesScreen::Hide() {
  if (view_)
    view_->Hide();
}

void DemoPreferencesScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionContinue) {
    Finish(ScreenExitCode::DEMO_MODE_PREFERENCES_CONTINUED);
  } else if (action_id == kUserActionClose) {
    Finish(ScreenExitCode::DEMO_MODE_PREFERENCES_CANCELED);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void DemoPreferencesScreen::OnViewDestroyed(DemoPreferencesScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

}  // namespace chromeos
