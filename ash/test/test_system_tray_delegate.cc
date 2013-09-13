// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_system_tray_delegate.h"

#include <string>

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"

namespace ash {
namespace test {

namespace {

user::LoginStatus g_initial_status = user::LOGGED_IN_USER;

}  // namespace

TestSystemTrayDelegate::TestSystemTrayDelegate()
    : should_show_display_notification_(false),
      login_status_(g_initial_status) {
}

TestSystemTrayDelegate::~TestSystemTrayDelegate() {
}

// static
void TestSystemTrayDelegate::SetInitialLoginStatus(
    user::LoginStatus login_status) {
  g_initial_status = login_status;
}

void TestSystemTrayDelegate::SetLoginStatus(user::LoginStatus login_status) {
  login_status_ = login_status;
  Shell::GetInstance()->UpdateAfterLoginStatusChange(login_status);
}

user::LoginStatus TestSystemTrayDelegate::GetUserLoginStatus() const {
  // Initial login status has been changed for testing.
  if (g_initial_status != user::LOGGED_IN_USER &&
      g_initial_status == login_status_) {
    return login_status_;
  }

  // At new user image screen manager->IsUserLoggedIn() would return true
  // but there's no browser session available yet so use SessionStarted().
  SessionStateDelegate* delegate =
      Shell::GetInstance()->session_state_delegate();

  if (!delegate->IsActiveUserSessionStarted())
    return ash::user::LOGGED_IN_NONE;
  if (delegate->IsScreenLocked())
    return user::LOGGED_IN_LOCKED;
  return login_status_;
}

bool TestSystemTrayDelegate::ShouldShowDisplayNotification() {
  return should_show_display_notification_;
}

void TestSystemTrayDelegate::ShutDown() {
  base::MessageLoop::current()->Quit();
}

void TestSystemTrayDelegate::SignOut() {
  base::MessageLoop::current()->Quit();
}

}  // namespace test
}  // namespace ash
