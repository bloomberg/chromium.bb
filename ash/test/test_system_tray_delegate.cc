// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_system_tray_delegate.h"

#include <string>

#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"

namespace ash {
namespace test {

namespace {

bool g_system_update_required = false;
LoginStatus g_initial_status = LoginStatus::USER;

}  // namespace

TestSystemTrayDelegate::TestSystemTrayDelegate()
    : should_show_display_notification_(false),
      login_status_(g_initial_status),
      session_length_limit_set_(false) {}

TestSystemTrayDelegate::~TestSystemTrayDelegate() {}

// static
void TestSystemTrayDelegate::SetInitialLoginStatus(LoginStatus login_status) {
  g_initial_status = login_status;
}

// static
void TestSystemTrayDelegate::SetSystemUpdateRequired(bool required) {
  g_system_update_required = required;
}

void TestSystemTrayDelegate::SetLoginStatus(LoginStatus login_status) {
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

LoginStatus TestSystemTrayDelegate::GetUserLoginStatus() const {
  // Initial login status has been changed for testing.
  if (g_initial_status != LoginStatus::USER &&
      g_initial_status == login_status_) {
    return login_status_;
  }

  // At new user image screen manager->IsUserLoggedIn() would return true
  // but there's no browser session available yet so use SessionStarted().
  SessionStateDelegate* delegate = WmShell::Get()->GetSessionStateDelegate();

  if (!delegate->IsActiveUserSessionStarted())
    return LoginStatus::NOT_LOGGED_IN;
  if (delegate->IsScreenLocked())
    return LoginStatus::LOCKED;
  return login_status_;
}

bool TestSystemTrayDelegate::IsUserSupervised() const {
  return login_status_ == LoginStatus::SUPERVISED;
}

void TestSystemTrayDelegate::GetSystemUpdateInfo(UpdateInfo* info) const {
  DCHECK(info);
  info->severity = UpdateInfo::UPDATE_NORMAL;
  info->update_required = g_system_update_required;
  info->factory_reset_required = false;
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

void TestSystemTrayDelegate::SignOut() {
  base::MessageLoop::current()->QuitWhenIdle();
}

}  // namespace test
}  // namespace ash
