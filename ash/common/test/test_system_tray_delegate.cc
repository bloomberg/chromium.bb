// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_system_tray_delegate.h"

#include <string>

#include "ash/common/login_status.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "base/time/time.h"

namespace ash {
namespace test {

namespace {

LoginStatus g_initial_status = LoginStatus::USER;

}  // namespace

TestSystemTrayDelegate::TestSystemTrayDelegate()
    : login_status_(g_initial_status), session_length_limit_set_(false) {}

TestSystemTrayDelegate::~TestSystemTrayDelegate() {}

void TestSystemTrayDelegate::SetLoginStatus(LoginStatus login_status) {
  login_status_ = login_status;
  WmShell::Get()->UpdateAfterLoginStatusChange(login_status);
}

void TestSystemTrayDelegate::SetSessionLengthLimitForTest(
    const base::TimeDelta& new_limit) {
  session_length_limit_ = new_limit;
  session_length_limit_set_ = true;
}

void TestSystemTrayDelegate::ClearSessionLengthLimit() {
  session_length_limit_set_ = false;
}

void TestSystemTrayDelegate::SetCurrentIME(const IMEInfo& info) {
  current_ime_ = info;
}

void TestSystemTrayDelegate::SetAvailableIMEList(const IMEInfoList& list) {
  ime_list_ = list;
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

void TestSystemTrayDelegate::GetCurrentIME(IMEInfo* info) {
  *info = current_ime_;
}

void TestSystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {
  *list = ime_list_;
}

////////////////////////////////////////////////////////////////////////////////

ScopedInitialLoginStatus::ScopedInitialLoginStatus(LoginStatus new_status)
    : old_status_(g_initial_status) {
  g_initial_status = new_status;
}

ScopedInitialLoginStatus::~ScopedInitialLoginStatus() {
  g_initial_status = old_status_;
}

}  // namespace test
}  // namespace ash
