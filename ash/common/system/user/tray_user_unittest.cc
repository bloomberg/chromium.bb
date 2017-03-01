// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/common/shell_delegate.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/system/user/tray_user.h"
#include "ash/common/system/user/user_view.h"
#include "ash/common/test/test_session_state_delegate.h"
#include "ash/common/wm_shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_shell_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/user_info.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/animation_container_element.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class TrayUserTest : public test::AshTestBase {
 public:
  TrayUserTest() = default;

  // testing::Test:
  void SetUp() override;

  // This has to be called prior to first use with the proper configuration.
  void InitializeParameters(int users_logged_in, bool multiprofile);

  // Show the system tray menu using the provided event generator.
  void ShowTrayMenu(ui::test::EventGenerator* generator);

  // Move the mouse over the user item.
  void MoveOverUserItem(ui::test::EventGenerator* generator, int index);

  // Click on the user item. Note that the tray menu needs to be shown.
  void ClickUserItem(ui::test::EventGenerator* generator, int index);

  // Accessors to various system components.
  SystemTray* tray() { return tray_; }
  test::TestSessionStateDelegate* delegate() { return delegate_; }
  TrayUser* tray_user(int index) { return tray_user_[index]; }

 private:
  SystemTray* tray_ = nullptr;
  test::TestSessionStateDelegate* delegate_ = nullptr;

  // Note that the ownership of these items is on the shelf.
  std::vector<TrayUser*> tray_user_;

  DISALLOW_COPY_AND_ASSIGN(TrayUserTest);
};

void TrayUserTest::SetUp() {
  test::AshTestBase::SetUp();
  tray_ = GetPrimarySystemTray();
  delegate_ = test::AshTestHelper::GetTestSessionStateDelegate();
}

void TrayUserTest::InitializeParameters(int users_logged_in,
                                        bool multiprofile) {
  // Set our default assumptions. Note that it is sufficient to set these
  // after everything was created.
  delegate_->set_logged_in_users(users_logged_in);
  test::TestShellDelegate* shell_delegate =
      static_cast<test::TestShellDelegate*>(WmShell::Get()->delegate());
  shell_delegate->set_multi_profiles_enabled(multiprofile);

  // Instead of using the existing tray panels we create new ones which makes
  // the access easier.
  for (int i = 0; i < delegate_->GetMaximumNumberOfLoggedInUsers(); i++) {
    tray_user_.push_back(new TrayUser(tray_, i));
    tray_->AddTrayItem(base::WrapUnique(tray_user_[i]));
  }
}

void TrayUserTest::ShowTrayMenu(ui::test::EventGenerator* generator) {
  gfx::Point center = tray()->GetBoundsInScreen().CenterPoint();

  generator->MoveMouseTo(center.x(), center.y());
  EXPECT_FALSE(tray()->IsSystemBubbleVisible());
  generator->ClickLeftButton();
}

void TrayUserTest::MoveOverUserItem(ui::test::EventGenerator* generator,
                                    int index) {
  gfx::Point center =
      tray_user(index)->GetUserPanelBoundsInScreenForTest().CenterPoint();

  generator->MoveMouseTo(center.x(), center.y());
}

void TrayUserTest::ClickUserItem(ui::test::EventGenerator* generator,
                                 int index) {
  MoveOverUserItem(generator, index);
  generator->ClickLeftButton();
}

}  // namespace

// Make sure that we show items for all users in the tray accordingly.
TEST_F(TrayUserTest, CheckTrayItemSize) {
  InitializeParameters(1, false);
  tray_user(0)->UpdateAfterLoginStatusChangeForTest(LoginStatus::GUEST);
  gfx::Size size = tray_user(0)->GetLayoutSizeForTest();
  EXPECT_EQ(kTrayItemSize, size.height());
  tray_user(0)->UpdateAfterLoginStatusChangeForTest(LoginStatus::USER);
  size = tray_user(0)->GetLayoutSizeForTest();
  EXPECT_EQ(kTrayItemSize, size.height());
}

// Make sure that in single user mode the user panel cannot be activated.
TEST_F(TrayUserTest, SingleUserModeDoesNotAllowAddingUser) {
  InitializeParameters(1, false);

  // Move the mouse over the status area and click to open the status menu.
  ui::test::EventGenerator& generator = GetEventGenerator();

  EXPECT_FALSE(tray()->IsSystemBubbleVisible());

  for (int i = 0; i < delegate()->GetMaximumNumberOfLoggedInUsers(); i++)
    EXPECT_EQ(TrayUser::HIDDEN, tray_user(i)->GetStateForTest());

  ShowTrayMenu(&generator);

  EXPECT_TRUE(tray()->HasSystemBubble());
  EXPECT_TRUE(tray()->IsSystemBubbleVisible());

  for (int i = 0; i < delegate()->GetMaximumNumberOfLoggedInUsers(); i++)
    EXPECT_EQ(i == 0 ? TrayUser::SHOWN : TrayUser::HIDDEN,
              tray_user(i)->GetStateForTest());
  tray()->CloseSystemBubble();
}

TEST_F(TrayUserTest, AccessibleLabelContainsSingleUserInfo) {
  InitializeParameters(1, false);
  ui::test::EventGenerator& generator = GetEventGenerator();
  ShowTrayMenu(&generator);

  views::View* view =
      tray_user(0)->user_view_for_test()->user_card_view_for_test();
  ui::AXNodeData node_data;
  view->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      base::UTF8ToUTF16("Über tray Über tray Über tray Über tray First@tray"),
      node_data.GetString16Attribute(ui::AX_ATTR_NAME));
  EXPECT_EQ(ui::AX_ROLE_STATIC_TEXT, node_data.role);
}

TEST_F(TrayUserTest, AccessibleLabelContainsMultiUserInfo) {
  InitializeParameters(1, true);
  ui::test::EventGenerator& generator = GetEventGenerator();
  ShowTrayMenu(&generator);

  views::View* view =
      tray_user(0)->user_view_for_test()->user_card_view_for_test();
  ui::AXNodeData node_data;
  view->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(
      base::UTF8ToUTF16("Über tray Über tray Über tray Über tray First@tray"),
      node_data.GetString16Attribute(ui::AX_ATTR_NAME));
  EXPECT_EQ(ui::AX_ROLE_BUTTON, node_data.role);
}

// Make sure that in multi user mode the user panel can be activated and there
// will be one panel for each user.
// Note: the mouse watcher (for automatic closing upon leave) cannot be tested
// here since it does not work with the event system in unit tests.
TEST_F(TrayUserTest, MultiUserModeDoesNotAllowToAddUser) {
  InitializeParameters(1, true);

  // Move the mouse over the status area and click to open the status menu.
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_async(false);

  int max_users = delegate()->GetMaximumNumberOfLoggedInUsers();
  // Checking now for each amount of users that the correct is done.
  for (int j = 1; j < max_users; j++) {
    // Set the number of logged in users.
    delegate()->set_logged_in_users(j);

    // Verify that nothing is shown.
    EXPECT_FALSE(tray()->IsSystemBubbleVisible());
    for (int i = 0; i < max_users; i++)
      EXPECT_FALSE(tray_user(i)->GetStateForTest());
    // After clicking on the tray the menu should get shown and for each logged
    // in user we should get a visible item.
    ShowTrayMenu(&generator);

    EXPECT_TRUE(tray()->HasSystemBubble());
    EXPECT_TRUE(tray()->IsSystemBubbleVisible());
    for (int i = 0; i < max_users; i++) {
      EXPECT_EQ(i < j ? TrayUser::SHOWN : TrayUser::HIDDEN,
                tray_user(i)->GetStateForTest());
    }

    // Move the mouse over the user item and it should hover.
    MoveOverUserItem(&generator, 0);
    EXPECT_EQ(TrayUser::HOVERED, tray_user(0)->GetStateForTest());
    for (int i = 1; i < max_users; i++) {
      EXPECT_EQ(i < j ? TrayUser::SHOWN : TrayUser::HIDDEN,
                tray_user(i)->GetStateForTest());
    }

    // Check that clicking the button allows to add item if we have still room
    // for one more user.
    ClickUserItem(&generator, 0);
    EXPECT_EQ(j == max_users ? TrayUser::ACTIVE_BUT_DISABLED : TrayUser::ACTIVE,
              tray_user(0)->GetStateForTest());

    // Click the button again to see that the menu goes away.
    ClickUserItem(&generator, 0);
    MoveOverUserItem(&generator, 0);
    EXPECT_EQ(TrayUser::HOVERED, tray_user(0)->GetStateForTest());

    // Close and check that everything is deleted.
    tray()->CloseSystemBubble();
    EXPECT_FALSE(tray()->IsSystemBubbleVisible());
    for (int i = 0; i < delegate()->GetMaximumNumberOfLoggedInUsers(); i++)
      EXPECT_EQ(TrayUser::HIDDEN, tray_user(i)->GetStateForTest());
  }
}

// Make sure that user changing gets properly executed.
TEST_F(TrayUserTest, MultiUserModeButtonClicks) {
  // Have two users.
  InitializeParameters(2, true);
  ui::test::EventGenerator& generator = GetEventGenerator();
  ShowTrayMenu(&generator);

  // Switch to a new user - which has a capitalized name.
  ClickUserItem(&generator, 1);
  const user_manager::UserInfo* active_user = delegate()->GetActiveUserInfo();
  const user_manager::UserInfo* second_user = delegate()->GetUserInfo(1);
  EXPECT_EQ(active_user->GetAccountId(), second_user->GetAccountId());
  // Since the name is capitalized, the email should be different than the
  // user_id.
  EXPECT_NE(active_user->GetAccountId().GetUserEmail(),
            second_user->GetDisplayEmail());
  tray()->CloseSystemBubble();
}

}  // namespace ash
