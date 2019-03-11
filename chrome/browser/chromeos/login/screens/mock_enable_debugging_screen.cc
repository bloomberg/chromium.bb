// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/mock_enable_debugging_screen.h"

namespace chromeos {

MockEnableDebuggingScreen::MockEnableDebuggingScreen(
    EnableDebuggingScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : EnableDebuggingScreen(view, exit_callback) {}

MockEnableDebuggingScreen::~MockEnableDebuggingScreen() {}

void MockEnableDebuggingScreen::ExitScreen() {
  exit_callback()->Run();
}

MockEnableDebuggingScreenView::MockEnableDebuggingScreenView() = default;

MockEnableDebuggingScreenView::~MockEnableDebuggingScreenView() {
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void MockEnableDebuggingScreenView::SetDelegate(
    EnableDebuggingScreenView::Delegate* delegate) {
  delegate_ = delegate;
  MockSetDelegate(delegate);
}

}  // namespace chromeos
