// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/multidevice_setup_screen_view.h"

namespace chromeos {

namespace {

constexpr const char kFinishedUserAction[] = "setup-finished";

}  // namespace

MultiDeviceSetupScreen::MultiDeviceSetupScreen(
    BaseScreenDelegate* base_screen_delegate,
    MultiDeviceSetupScreenView* view)
    : BaseScreen(base_screen_delegate, OobeScreen::SCREEN_MULTIDEVICE_SETUP),
      view_(view) {
  DCHECK(view_);
  view_->Bind(this);
}

MultiDeviceSetupScreen::~MultiDeviceSetupScreen() {
  view_->Bind(nullptr);
}

void MultiDeviceSetupScreen::Show() {
  view_->Show();
}

void MultiDeviceSetupScreen::Hide() {
  view_->Hide();
}

void MultiDeviceSetupScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kFinishedUserAction) {
    Finish(ScreenExitCode::MULTIDEVICE_SETUP_FINISHED);
    return;
  }

  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
