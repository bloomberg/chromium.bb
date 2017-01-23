// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/session/session_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/common/session/session_controller.h"
#include "ash/common/session/session_state_observer.h"
#include "ash/public/interfaces/session_controller.mojom.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class TestSessionStateObserver : public SessionStateObserver {
 public:
  TestSessionStateObserver() : active_account_id_(EmptyAccountId()) {}
  ~TestSessionStateObserver() override {}

  // SessionStateObserver:
  void ActiveUserChanged(const AccountId& account_id) override {
    active_account_id_ = account_id;
  }

  void UserAddedToSession(const AccountId& account_id) override {
    user_session_account_ids_.push_back(account_id);
  }

  void SessionStateChanged(session_manager::SessionState state) override {
    state_ = state;
  }

  std::string GetUserSessionEmails() const {
    std::string emails;
    for (const auto& account_id : user_session_account_ids_) {
      emails += account_id.GetUserEmail() + ",";
    }
    return emails;
  }

  session_manager::SessionState state() const { return state_; }
  const AccountId& active_account_id() const { return active_account_id_; }
  const std::vector<AccountId>& user_session_account_ids() const {
    return user_session_account_ids_;
  }

 private:
  session_manager::SessionState state_ = session_manager::SessionState::UNKNOWN;
  AccountId active_account_id_;
  std::vector<AccountId> user_session_account_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestSessionStateObserver);
};

void FillDefaultSessionInfo(mojom::SessionInfo* info) {
  info->can_lock_screen = true;
  info->should_lock_screen_automatically = true;
  info->add_user_session_policy = AddUserSessionPolicy::ALLOWED;
  info->state = session_manager::SessionState::LOGIN_PRIMARY;
}

class SessionControllerTest : public testing::Test {
 public:
  SessionControllerTest() {}
  ~SessionControllerTest() override {}

  // testing::Test:
  void SetUp() override {
    controller_ = base::MakeUnique<SessionController>();
    controller_->AddSessionStateObserver(&observer_);
  }

  void TearDown() override {
    controller_->RemoveSessionStateObserver(&observer_);
  }

  void SetSessionInfo(const mojom::SessionInfo& info) {
    mojom::SessionInfoPtr info_ptr = mojom::SessionInfo::New();
    *info_ptr = info;
    controller_->SetSessionInfo(std::move(info_ptr));
  }

  void UpdateSession(uint32_t session_id, const std::string& email) {
    mojom::UserSessionPtr session = mojom::UserSession::New();
    session->session_id = session_id;
    session->type = user_manager::USER_TYPE_REGULAR;
    session->account_id = AccountId::FromUserEmail(email);
    session->display_name = email;
    session->display_email = email;

    controller_->UpdateUserSession(std::move(session));
  }

  std::string GetUserSessionEmails() const {
    std::string emails;
    for (const auto& session : controller_->GetUserSessions()) {
      emails += session->display_email + ",";
    }
    return emails;
  }

  SessionController* controller() { return controller_.get(); }
  const TestSessionStateObserver* observer() const { return &observer_; }

 private:
  std::unique_ptr<SessionController> controller_;
  TestSessionStateObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(SessionControllerTest);
};

// Tests that the simple session info is reflected properly.
TEST_F(SessionControllerTest, SimpleSessionInfo) {
  mojom::SessionInfo info;
  FillDefaultSessionInfo(&info);
  SetSessionInfo(info);

  EXPECT_EQ(session_manager::kMaxmiumNumberOfUserSessions,
            controller()->GetMaximumNumberOfLoggedInUsers());
  EXPECT_TRUE(controller()->CanLockScreen());
  EXPECT_TRUE(controller()->ShouldLockScreenAutomatically());

  info.can_lock_screen = false;
  SetSessionInfo(info);
  EXPECT_FALSE(controller()->CanLockScreen());
  EXPECT_TRUE(controller()->ShouldLockScreenAutomatically());

  info.should_lock_screen_automatically = false;
  SetSessionInfo(info);
  EXPECT_FALSE(controller()->CanLockScreen());
  EXPECT_FALSE(controller()->ShouldLockScreenAutomatically());
}

// Tests that AddUserSessionPolicy is set properly.
TEST_F(SessionControllerTest, AddUserPolicy) {
  const AddUserSessionPolicy kTestCases[] = {
      AddUserSessionPolicy::ALLOWED,
      AddUserSessionPolicy::ERROR_NOT_ALLOWED_PRIMARY_USER,
      AddUserSessionPolicy::ERROR_NO_ELIGIBLE_USERS,
      AddUserSessionPolicy::ERROR_MAXIMUM_USERS_REACHED,
  };

  mojom::SessionInfo info;
  FillDefaultSessionInfo(&info);
  for (const auto& policy : kTestCases) {
    info.add_user_session_policy = policy;
    SetSessionInfo(info);
    EXPECT_EQ(policy, controller()->GetAddUserPolicy())
        << "Test case policy=" << static_cast<int>(policy);
  }
}

// Tests that session state can be set and reflected properly.
TEST_F(SessionControllerTest, SessionState) {
  const struct {
    session_manager::SessionState state;
    bool expected_is_screen_locked;
    bool expected_is_user_session_blocked;
  } kTestCases[] = {
      {session_manager::SessionState::OOBE, false, true},
      {session_manager::SessionState::LOGIN_PRIMARY, false, true},
      {session_manager::SessionState::LOGGED_IN_NOT_ACTIVE, false, true},
      {session_manager::SessionState::ACTIVE, false, false},
      {session_manager::SessionState::LOCKED, true, true},
      {session_manager::SessionState::LOGIN_SECONDARY, false, true},
  };

  mojom::SessionInfo info;
  FillDefaultSessionInfo(&info);
  for (const auto& test_case : kTestCases) {
    info.state = test_case.state;
    SetSessionInfo(info);

    EXPECT_EQ(test_case.state, controller()->GetSessionState())
        << "Test case state=" << static_cast<int>(test_case.state);
    EXPECT_EQ(observer()->state(), controller()->GetSessionState())
        << "Test case state=" << static_cast<int>(test_case.state);
    EXPECT_EQ(test_case.expected_is_screen_locked,
              controller()->IsScreenLocked())
        << "Test case state=" << static_cast<int>(test_case.state);
    EXPECT_EQ(test_case.expected_is_user_session_blocked,
              controller()->IsUserSessionBlocked())
        << "Test case state=" << static_cast<int>(test_case.state);
  }
}

// Tests that user sessions can be set and updated.
TEST_F(SessionControllerTest, UserSessions) {
  EXPECT_FALSE(controller()->IsActiveUserSessionStarted());

  UpdateSession(1u, "user1@test.com");
  EXPECT_TRUE(controller()->IsActiveUserSessionStarted());
  EXPECT_EQ("user1@test.com,", GetUserSessionEmails());
  EXPECT_EQ(GetUserSessionEmails(), observer()->GetUserSessionEmails());

  UpdateSession(2u, "user2@test.com");
  EXPECT_TRUE(controller()->IsActiveUserSessionStarted());
  EXPECT_EQ("user1@test.com,user2@test.com,", GetUserSessionEmails());
  EXPECT_EQ(GetUserSessionEmails(), observer()->GetUserSessionEmails());

  UpdateSession(1u, "user1_changed@test.com");
  EXPECT_EQ("user1_changed@test.com,user2@test.com,", GetUserSessionEmails());
  // TODO(xiyuan): Verify observer gets the updated user session info.
}

// Tests that user sessions can be ordered.
TEST_F(SessionControllerTest, ActiveSession) {
  UpdateSession(1u, "user1@test.com");
  UpdateSession(2u, "user2@test.com");

  std::vector<uint32_t> order = {1u, 2u};
  controller()->SetUserSessionOrder(order);
  EXPECT_EQ("user1@test.com,user2@test.com,", GetUserSessionEmails());
  EXPECT_EQ("user1@test.com", observer()->active_account_id().GetUserEmail());

  order = {2u, 1u};
  controller()->SetUserSessionOrder(order);
  EXPECT_EQ("user2@test.com,user1@test.com,", GetUserSessionEmails());
  EXPECT_EQ("user2@test.com", observer()->active_account_id().GetUserEmail());

  order = {1u, 2u};
  controller()->SetUserSessionOrder(order);
  EXPECT_EQ("user1@test.com,user2@test.com,", GetUserSessionEmails());
  EXPECT_EQ("user1@test.com", observer()->active_account_id().GetUserEmail());
}

}  // namespace
}  // namespace ash
