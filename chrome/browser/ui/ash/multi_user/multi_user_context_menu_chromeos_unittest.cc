// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_context_menu.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/ui_base_types.h"

namespace ash {
namespace test {

// A test class for preparing the chrome::MultiUserContextMenu.
class MultiUserContextMenuChromeOSTest : public AshTestBase {
 public:
  MultiUserContextMenuChromeOSTest() : multi_user_window_manager_(NULL) {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  // Set up the test environment for this many windows.
  void SetUpForThisManyWindows(int windows);

  aura::Window* window() { return window_; }
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager() {
    return multi_user_window_manager_;
  }

  void SetNumberOfUsers(int users) {
    ash::test::TestSessionStateDelegate* delegate =
        static_cast<ash::test::TestSessionStateDelegate*>(
            ash::Shell::GetInstance()->session_state_delegate());
    delegate->set_logged_in_users(users);
  }

 private:
  // A window which can be used for testing.
  aura::Window* window_;

  // The instance of the MultiUserWindowManager.
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserContextMenuChromeOSTest);
};

void MultiUserContextMenuChromeOSTest::SetUp() {
  AshTestBase::SetUp();

  window_ = CreateTestWindowInShellWithId(0);
  window_->Show();

  multi_user_window_manager_ = new chrome::MultiUserWindowManagerChromeOS("A");
  chrome::MultiUserWindowManager::SetInstanceForTest(multi_user_window_manager_,
        chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED);
  EXPECT_TRUE(multi_user_window_manager_);
}

void MultiUserContextMenuChromeOSTest::TearDown() {
  delete window_;

  chrome::MultiUserWindowManager::DeleteInstance();
  AshTestBase::TearDown();
}

// Check that an unowned window will never create a menu.
TEST_F(MultiUserContextMenuChromeOSTest, UnownedWindow) {
  EXPECT_EQ(NULL, CreateMultiUserContextMenu(window()).get());

  // Add more users.
  SetNumberOfUsers(2);
  EXPECT_EQ(NULL, CreateMultiUserContextMenu(window()).get());
}

// Check that an owned window will never create a menu.
TEST_F(MultiUserContextMenuChromeOSTest, OwnedWindow) {
  // Make the window owned and check that there is no menu (since only a single
  // user exists).
  multi_user_window_manager()->SetWindowOwner(window(), "A");
  EXPECT_EQ(NULL, CreateMultiUserContextMenu(window()).get());

  // After adding another user a menu should get created.
  {
    SetNumberOfUsers(2);
    scoped_ptr<ui::MenuModel> menu = CreateMultiUserContextMenu(window());
    ASSERT_TRUE(menu.get());
    EXPECT_EQ(1, menu.get()->GetItemCount());
  }
  {
    SetNumberOfUsers(3);
    scoped_ptr<ui::MenuModel> menu = CreateMultiUserContextMenu(window());
    ASSERT_TRUE(menu.get());
    EXPECT_EQ(2, menu.get()->GetItemCount());
  }
}

}  // namespace test
}  // namespace ash
