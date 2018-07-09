// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/demo_preferences_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/demo_preferences_screen_view.h"

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
    // TODO(agawronska): Add continue action.
    NOTIMPLEMENTED();
  } else if (action_id == kUserActionClose) {
    // TODO(agawronska): Add close action.
    NOTIMPLEMENTED();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void DemoPreferencesScreen::OnViewDestroyed(DemoPreferencesScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

}  // namespace chromeos
