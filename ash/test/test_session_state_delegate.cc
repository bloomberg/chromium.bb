// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_session_state_delegate.h"

#include <algorithm>
#include <string>

#include "ash/shell.h"
#include "ash/system/user/login_status.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace test {

namespace {

// The the "canonicalized" user ID from a given |email| address.
std::string GetUserIDFromEmail(const std::string& email) {
  std::string user_id = email;
  std::transform(user_id.begin(), user_id.end(), user_id.begin(), ::tolower);
  return user_id;
}

}  // namespace

class MockUserInfo : public user_manager::UserInfo {
 public:
  explicit MockUserInfo(const std::string& id) : email_(id) {}
  virtual ~MockUserInfo() {}

  void SetUserImage(const gfx::ImageSkia& user_image) {
    user_image_ = user_image;
  }

  virtual base::string16 GetDisplayName() const OVERRIDE {
    return base::UTF8ToUTF16("Über tray Über tray Über tray Über tray");
  }

  virtual base::string16 GetGivenName() const OVERRIDE {
    return base::UTF8ToUTF16("Über Über Über Über");
  }

  virtual std::string GetEmail() const OVERRIDE { return email_; }

  virtual std::string GetUserID() const OVERRIDE {
    return GetUserIDFromEmail(GetEmail());
  }

  virtual const gfx::ImageSkia& GetImage() const OVERRIDE {
    return user_image_;
  }

  // A test user image.
  gfx::ImageSkia user_image_;

  std::string email_;

  DISALLOW_COPY_AND_ASSIGN(MockUserInfo);
};

TestSessionStateDelegate::TestSessionStateDelegate()
    : has_active_user_(false),
      active_user_session_started_(false),
      can_lock_screen_(true),
      should_lock_screen_before_suspending_(false),
      screen_locked_(false),
      user_adding_screen_running_(false),
      logged_in_users_(1),
      active_user_index_(0) {
  user_list_.push_back(
      new MockUserInfo("First@tray"));  // This is intended to be capitalized.
  user_list_.push_back(
      new MockUserInfo("Second@tray"));  // This is intended to be capitalized.
  user_list_.push_back(new MockUserInfo("third@tray"));
  user_list_.push_back(new MockUserInfo("someone@tray"));
}

TestSessionStateDelegate::~TestSessionStateDelegate() {
  STLDeleteElements(&user_list_);
}

void TestSessionStateDelegate::AddUser(const std::string user_id) {
  user_list_.push_back(new MockUserInfo(user_id));
}

const user_manager::UserInfo* TestSessionStateDelegate::GetActiveUserInfo()
    const {
  return user_list_[active_user_index_];
}

content::BrowserContext*
TestSessionStateDelegate::GetBrowserContextByIndex(
    MultiProfileIndex index) {
  return NULL;
}

content::BrowserContext*
TestSessionStateDelegate::GetBrowserContextForWindow(
    aura::Window* window) {
  return NULL;
}

int TestSessionStateDelegate::GetMaximumNumberOfLoggedInUsers() const {
  return 3;
}

int TestSessionStateDelegate::NumberOfLoggedInUsers() const {
  // TODO(skuhne): Add better test framework to test multiple profiles.
  return has_active_user_ ? logged_in_users_ : 0;
}

bool TestSessionStateDelegate::IsActiveUserSessionStarted() const {
  return active_user_session_started_;
}

bool TestSessionStateDelegate::CanLockScreen() const {
  return has_active_user_ && can_lock_screen_;
}

bool TestSessionStateDelegate::IsScreenLocked() const {
  return screen_locked_;
}

bool TestSessionStateDelegate::ShouldLockScreenBeforeSuspending() const {
  return should_lock_screen_before_suspending_;
}

void TestSessionStateDelegate::LockScreen() {
  if (CanLockScreen())
    screen_locked_ = true;
}

void TestSessionStateDelegate::UnlockScreen() {
  screen_locked_ = false;
}

bool TestSessionStateDelegate::IsUserSessionBlocked() const {
  return !IsActiveUserSessionStarted() || IsScreenLocked() ||
      user_adding_screen_running_;
}

SessionStateDelegate::SessionState TestSessionStateDelegate::GetSessionState()
    const {
  if (user_adding_screen_running_)
    return SESSION_STATE_LOGIN_SECONDARY;

  // Assuming that if session is not active we're at login.
  return IsActiveUserSessionStarted() ?
      SESSION_STATE_ACTIVE : SESSION_STATE_LOGIN_PRIMARY;
}

void TestSessionStateDelegate::SetHasActiveUser(bool has_active_user) {
  has_active_user_ = has_active_user;
  if (!has_active_user)
    active_user_session_started_ = false;
  else
    Shell::GetInstance()->ShowShelf();
}

void TestSessionStateDelegate::SetActiveUserSessionStarted(
    bool active_user_session_started) {
  active_user_session_started_ = active_user_session_started;
  if (active_user_session_started) {
    has_active_user_ = true;
    Shell::GetInstance()->CreateShelf();
    Shell::GetInstance()->UpdateAfterLoginStatusChange(
        user::LOGGED_IN_USER);
  }
}

void TestSessionStateDelegate::SetCanLockScreen(bool can_lock_screen) {
  can_lock_screen_ = can_lock_screen;
}

void TestSessionStateDelegate::SetShouldLockScreenBeforeSuspending(
    bool should_lock) {
  should_lock_screen_before_suspending_ = should_lock;
}

void TestSessionStateDelegate::SetUserAddingScreenRunning(
    bool user_adding_screen_running) {
  user_adding_screen_running_ = user_adding_screen_running;
}

void TestSessionStateDelegate::SetUserImage(
    const gfx::ImageSkia& user_image) {
  user_list_[active_user_index_]->SetUserImage(user_image);
}

const user_manager::UserInfo* TestSessionStateDelegate::GetUserInfo(
    MultiProfileIndex index) const {
  int max = static_cast<int>(user_list_.size());
  return user_list_[index < max ? index : max - 1];
}

const user_manager::UserInfo* TestSessionStateDelegate::GetUserInfo(
    content::BrowserContext* context) const {
  return user_list_[active_user_index_];
}

bool TestSessionStateDelegate::ShouldShowAvatar(aura::Window* window) const {
  return !GetActiveUserInfo()->GetImage().isNull();
}

void TestSessionStateDelegate::SwitchActiveUser(const std::string& user_id) {
  // Make sure this is a user id and not an email address.
  EXPECT_EQ(user_id, GetUserIDFromEmail(user_id));
  active_user_index_ = 0;
  for (std::vector<MockUserInfo*>::iterator iter = user_list_.begin();
       iter != user_list_.end();
       ++iter) {
    if ((*iter)->GetUserID() == user_id) {
      active_user_index_ = iter - user_list_.begin();
      return;
    }
  }
  NOTREACHED() << "Unknown user:" << user_id;
}

void TestSessionStateDelegate::CycleActiveUser(CycleUser cycle_user) {
  SwitchActiveUser("someone@tray");
}

bool TestSessionStateDelegate::IsMultiProfileAllowedByPrimaryUserPolicy()
    const {
  return true;
}

void TestSessionStateDelegate::AddSessionStateObserver(
    SessionStateObserver* observer) {
}

void TestSessionStateDelegate::RemoveSessionStateObserver(
    SessionStateObserver* observer) {
}

}  // namespace test
}  // namespace ash
