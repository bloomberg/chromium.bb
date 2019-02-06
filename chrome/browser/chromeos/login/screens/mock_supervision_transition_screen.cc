// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_supervision_transition_screen.h"

namespace chromeos {

MockSupervisionTransitionScreen::MockSupervisionTransitionScreen(
    BaseScreenDelegate* base_screen_delegate,
    SupervisionTransitionScreenView* view)
    : SupervisionTransitionScreen(base_screen_delegate, view) {}

MockSupervisionTransitionScreen::~MockSupervisionTransitionScreen() = default;

MockSupervisionTransitionScreenView::MockSupervisionTransitionScreenView() =
    default;

MockSupervisionTransitionScreenView::~MockSupervisionTransitionScreenView() {
  if (screen_)
    screen_->OnViewDestroyed(this);
}

void MockSupervisionTransitionScreenView::Bind(
    SupervisionTransitionScreen* screen) {
  screen_ = screen;
  MockBind(screen);
}

void MockSupervisionTransitionScreenView::Unbind() {
  screen_ = nullptr;
  MockUnbind();
}

}  // namespace chromeos
