// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"

#include "chrome/browser/chromeos/login/ui/login_display_host_impl.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

OobeScreenWaiter::OobeScreenWaiter(OobeDisplay::Screen expected_screen)
    : waiting_for_screen_(false),
      expected_screen_(expected_screen) {
}

OobeScreenWaiter::~OobeScreenWaiter() {
  if (waiting_for_screen_) {
    GetOobeUI()->RemoveObserver(this);
  }
}

void OobeScreenWaiter::Wait() {
  WaitNoAssertCurrentScreen();

  ASSERT_EQ(expected_screen_, GetOobeUI()->current_screen());
}

void OobeScreenWaiter::WaitNoAssertCurrentScreen() {
  if (GetOobeUI()->current_screen() == expected_screen_)
    return;

  waiting_for_screen_ = true;
  GetOobeUI()->AddObserver(this);

  runner_ = new content::MessageLoopRunner;
  runner_->Run();
  ASSERT_FALSE(waiting_for_screen_);
}

void OobeScreenWaiter::OnCurrentScreenChanged(
    OobeDisplay::Screen current_screen,
    OobeDisplay::Screen new_screen) {
  if (waiting_for_screen_ && new_screen == expected_screen_) {
    runner_->Quit();
    waiting_for_screen_ = false;
    GetOobeUI()->RemoveObserver(this);
  }
}

OobeUI* OobeScreenWaiter::GetOobeUI() {
  OobeUI* oobe_ui = static_cast<chromeos::LoginDisplayHostImpl*>(
      chromeos::LoginDisplayHostImpl::default_host())->GetOobeUI();
  CHECK(oobe_ui);
  return oobe_ui;
}

}  // namespace chromeos
