// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_system_tray_delegate.h"

#include <string>

#include "ash/session/session_state_delegate.h"
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
      login_status_(g_initial_status),
      session_length_limit_set_(false) {
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

void TestSystemTrayDelegate::SetSessionLengthLimitForTest(
    const base::TimeDelta& new_limit) {
  session_length_limit_ = new_limit;
  session_length_limit_set_ = true;
}

void TestSystemTrayDelegate::ClearSessionLengthLimit() {
  session_length_limit_set_ = false;
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

bool TestSystemTrayDelegate::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  // Just returns TimeTicks::Now(), so the remaining time is always the
  // specified limit. This is useful for testing.
  if (session_length_limit_set_)
    *session_start_time = base::TimeTicks::Now();
  return session_length_limit_set_;
}

bool TestSystemTrayDelegate::GetSessionLengthLimit(
    base::TimeDelta* session_length_limit) {
  if (session_length_limit_set_)
    *session_length_limit = session_length_limit_;
  return session_length_limit_set_;
}

void TestSystemTrayDelegate::ShutDown() {
  base::MessageLoop::current()->Quit();
}

void TestSystemTrayDelegate::SignOut() {
  base::MessageLoop::current()->Quit();
}

}  // namespace test
}  // namespace ash
