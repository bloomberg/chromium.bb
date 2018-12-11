// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/supervision_transition_screen.h"

#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/supervision_transition_screen_view.h"

namespace chromeos {

SupervisionTransitionScreen::SupervisionTransitionScreen(
    BaseScreenDelegate* base_screen_delegate,
    SupervisionTransitionScreenView* view)
    : BaseScreen(base_screen_delegate,
                 OobeScreen::SCREEN_SUPERVISION_TRANSITION),
      view_(view) {
  if (view_)
    view_->Bind(this);
}

SupervisionTransitionScreen::~SupervisionTransitionScreen() {
  if (view_)
    view_->Unbind();
}

void SupervisionTransitionScreen::Show() {
  if (view_)
    view_->Show();
}

void SupervisionTransitionScreen::Hide() {
  if (view_)
    view_->Hide();
}

void SupervisionTransitionScreen::OnViewDestroyed(
    SupervisionTransitionScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

void SupervisionTransitionScreen::OnSupervisionTransitionFinished() {
  Finish(ScreenExitCode::SUPERVISION_TRANSITION_FINISHED);
}

}  // namespace chromeos
