// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_session_state_delegate.h"

#include <algorithm>
#include <string>

#include "ash/common/login_status.h"
#include "ash/common/wm_shell.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace test {

namespace {

// Returns the "canonicalized" email from a given |email| address.
std::string GetUserIdFromEmail(const std::string& email) {
  std::string user_id = email;
  std::transform(user_id.begin(), user_id.end(), user_id.begin(), ::tolower);
  return user_id;
}

// Returns Account ID from a given |email| address.
AccountId GetAccountIdFromEmail(const std::string& email) {
  return AccountId::FromUserEmail(GetUserIdFromEmail(email));
}

}  // namespace

class MockUserInfo : public user_manager::UserInfo {
 public:
  explicit MockUserInfo(const std::string& display_email)
      : display_email_(display_email),
        account_id_(GetAccountIdFromEmail(display_email)) {}
  ~MockUserInfo() override {}

  void SetUserImage(const gfx::ImageSkia& user_image) {
    user_image_ = user_image;
  }

  base::string16 GetDisplayName() const override {
    return base::UTF8ToUTF16("Über tray Über tray Über tray Über tray");
  }

  base::string16 GetGivenName() const override {
    return base::UTF8ToUTF16("Über Über Über Über");
  }

  std::string GetDisplayEmail() const override { return display_email_; }

  const AccountId& GetAccountId() const override { return account_id_; }

  const gfx::ImageSkia& GetImage() const override { return user_image_; }

  // A test user image.
  gfx::ImageSkia user_image_;

  std::string display_email_;
  const AccountId account_id_;

  DISALLOW_COPY_AND_ASSIGN(MockUserInfo);
};

// A test version of user_manager::UserManager which can be used for testing on
// non-ChromeOS builds.
class TestSessionStateDelegate::TestUserManager {
 public:
  TestUserManager() : session_started_(false) {}

  void SessionStarted() { session_started_ = true; }

  bool IsSessionStarted() const { return session_started_; }

 private:
  // True if SessionStarted() has been called.
  bool session_started_;
  DISALLOW_COPY_AND_ASSIGN(TestUserManager);
};

TestSessionStateDelegate::TestSessionStateDelegate()
    : can_lock_screen_(true),
      should_lock_screen_automatically_(false),
      screen_locked_(false),
      user_adding_screen_running_(false),
      logged_in_users_(1),
      active_user_index_(0),
      user_manager_(new TestUserManager()),
      session_state_(session_manager::SessionState::LOGIN_PRIMARY) {
  // This is intended to be capitalized.
  user_list_.push_back(base::MakeUnique<MockUserInfo>("First@tray"));
  // This is intended to be capitalized.
  user_list_.push_back(base::MakeUnique<MockUserInfo>("Second@tray"));
  user_list_.push_back(base::MakeUnique<MockUserInfo>("third@tray"));
  user_list_.push_back(base::MakeUnique<MockUserInfo>("someone@tray"));
}

TestSessionStateDelegate::~TestSessionStateDelegate() {}

void TestSessionStateDelegate::AddUser(const AccountId& account_id) {
  user_list_.push_back(
      base::MakeUnique<MockUserInfo>(account_id.GetUserEmail()));
}

const user_manager::UserInfo* TestSessionStateDelegate::GetActiveUserInfo()
    const {
  return user_list_[active_user_index_].get();
}

int TestSessionStateDelegate::GetMaximumNumberOfLoggedInUsers() const {
  return 3;
}

int TestSessionStateDelegate::NumberOfLoggedInUsers() const {
  // TODO(skuhne): Add better test framework to test multiple profiles.
  return IsActiveUserSessionStarted() ? logged_in_users_ : 0;
}

bool TestSessionStateDelegate::IsActiveUserSessionStarted() const {
  return user_manager_->IsSessionStarted() &&
         session_state_ == session_manager::SessionState::ACTIVE;
}

bool TestSessionStateDelegate::CanLockScreen() const {
  return IsActiveUserSessionStarted() && can_lock_screen_;
}

bool TestSessionStateDelegate::IsScreenLocked() const {
  return screen_locked_;
}

bool TestSessionStateDelegate::ShouldLockScreenAutomatically() const {
  return should_lock_screen_automatically_;
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
         user_adding_screen_running_ ||
         session_state_ != session_manager::SessionState::ACTIVE;
}

session_manager::SessionState TestSessionStateDelegate::GetSessionState()
    const {
  return session_state_;
}

void TestSessionStateDelegate::SetHasActiveUser(bool has_active_user) {
  session_state_ = has_active_user
                       ? session_manager::SessionState::ACTIVE
                       : session_manager::SessionState::LOGIN_PRIMARY;
}

void TestSessionStateDelegate::SetActiveUserSessionStarted(
    bool active_user_session_started) {
  if (active_user_session_started) {
    user_manager_->SessionStarted();
    session_state_ = session_manager::SessionState::ACTIVE;
    WmShell::Get()->CreateShelfView();
    WmShell::Get()->UpdateAfterLoginStatusChange(LoginStatus::USER);
  } else {
    session_state_ = session_manager::SessionState::LOGIN_PRIMARY;
    user_manager_.reset(new TestUserManager());
  }
}

// static
void TestSessionStateDelegate::SetCanLockScreen(bool can_lock_screen) {
  CHECK(WmShell::HasInstance());
  static_cast<ash::test::TestSessionStateDelegate*>(
      WmShell::Get()->GetSessionStateDelegate())
      ->can_lock_screen_ = can_lock_screen;
}

void TestSessionStateDelegate::SetShouldLockScreenAutomatically(
    bool should_lock) {
  should_lock_screen_automatically_ = should_lock;
}

void TestSessionStateDelegate::SetUserAddingScreenRunning(
    bool user_adding_screen_running) {
  user_adding_screen_running_ = user_adding_screen_running;
  if (user_adding_screen_running_)
    session_state_ = session_manager::SessionState::LOGIN_SECONDARY;
  else
    session_state_ = session_manager::SessionState::ACTIVE;
}

void TestSessionStateDelegate::SetUserImage(const gfx::ImageSkia& user_image) {
  user_list_[active_user_index_]->SetUserImage(user_image);
}

const user_manager::UserInfo* TestSessionStateDelegate::GetUserInfo(
    UserIndex index) const {
  int max = static_cast<int>(user_list_.size());
  return user_list_[index < max ? index : max - 1].get();
}

bool TestSessionStateDelegate::ShouldShowAvatar(WmWindow* window) const {
  return !GetActiveUserInfo()->GetImage().isNull();
}

gfx::ImageSkia TestSessionStateDelegate::GetAvatarImageForWindow(
    WmWindow* window) const {
  return gfx::ImageSkia();
}

void TestSessionStateDelegate::SwitchActiveUser(const AccountId& account_id) {
  // Make sure this is a user id and not an email address.
  EXPECT_EQ(account_id.GetUserEmail(),
            GetUserIdFromEmail(account_id.GetUserEmail()));
  active_user_index_ = 0;
  for (auto iter = user_list_.begin(); iter != user_list_.end(); ++iter) {
    if ((*iter)->GetAccountId() == account_id) {
      active_user_index_ = iter - user_list_.begin();
      return;
    }
  }
  NOTREACHED() << "Unknown user:" << account_id.GetUserEmail();
}

void TestSessionStateDelegate::CycleActiveUser(CycleUserDirection direction) {
  SwitchActiveUser(AccountId::FromUserEmail("someone@tray"));
}

bool TestSessionStateDelegate::IsMultiProfileAllowedByPrimaryUserPolicy()
    const {
  return true;
}

void TestSessionStateDelegate::AddSessionStateObserver(
    SessionStateObserver* observer) {}

void TestSessionStateDelegate::RemoveSessionStateObserver(
    SessionStateObserver* observer) {}

}  // namespace test
}  // namespace ash
