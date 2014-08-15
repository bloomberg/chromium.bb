// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/extensions/wallpaper_private_api.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"

//#include "base/compiler_specific.h"
//#include "base/logging.h"
//#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
namespace chromeos {
namespace {

const char* kTestAccount1 = "user1@test.com";
const char* kTestAccount2 = "user2@test.com";

class WallpaperPrivateApiUnittest : public ash::test::AshTestBase {
 public:
  WallpaperPrivateApiUnittest()
      : fake_user_manager_(new FakeUserManager()),
        scoped_user_manager_(fake_user_manager_) {
  }

 protected:
  FakeUserManager* fake_user_manager() {
    return fake_user_manager_;
  }

 private:
  FakeUserManager* fake_user_manager_;
  ScopedUserManagerEnabler scoped_user_manager_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperPrivateApiUnittest);
};

class TestMinimizeFunction
    : public WallpaperPrivateMinimizeInactiveWindowsFunction {
 public:
  TestMinimizeFunction() {}

  virtual bool RunAsync() OVERRIDE {
    return WallpaperPrivateMinimizeInactiveWindowsFunction::RunAsync();
  }

 protected:
  virtual ~TestMinimizeFunction() {}
};

class TestRestoreFunction
    : public WallpaperPrivateRestoreMinimizedWindowsFunction {
 public:
  TestRestoreFunction() {}

  virtual bool RunAsync() OVERRIDE {
    return WallpaperPrivateRestoreMinimizedWindowsFunction::RunAsync();
  }
 protected:
  virtual ~TestRestoreFunction() {}
};

}  // namespace

TEST_F(WallpaperPrivateApiUnittest, HideAndRestoreWindows) {
  fake_user_manager()->AddUser(kTestAccount1);
  scoped_ptr<aura::Window> window3(CreateTestWindowInShellWithId(3));
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  scoped_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));

  ash::wm::WindowState* window0_state = ash::wm::GetWindowState(window0.get());
  ash::wm::WindowState* window1_state = ash::wm::GetWindowState(window1.get());
  ash::wm::WindowState* window2_state = ash::wm::GetWindowState(window2.get());
  ash::wm::WindowState* window3_state = ash::wm::GetWindowState(window3.get());

  window3_state->Minimize();
  window1_state->Maximize();

  // Window 3 starts minimized, window 1 starts maximized.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_TRUE(window3_state->IsMinimized());

  // We then activate window 0 (i.e. wallpaper picker) and call the minimize
  // function.
  window0_state->Activate();
  EXPECT_TRUE(window0_state->IsActive());
  scoped_refptr<TestMinimizeFunction> minimize_function(
      new TestMinimizeFunction());
  EXPECT_TRUE(minimize_function->RunAsync());

  // All windows except window 0 should be minimized.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());
  EXPECT_TRUE(window2_state->IsMinimized());
  EXPECT_TRUE(window3_state->IsMinimized());

  // Then we destroy window 0 and call the restore function.
  window0.reset();
  scoped_refptr<TestRestoreFunction> restore_function(
      new TestRestoreFunction());
  EXPECT_TRUE(restore_function->RunAsync());

  // Windows 1 and 2 should no longer be minimized. Window 1 should again
  // be maximized. Window 3 should still be minimized.
  EXPECT_TRUE(window1_state->IsMaximized());
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_TRUE(window3_state->IsMinimized());
}

// Test for multiple calls to |MinimizeInactiveWindows| before call
// |RestoreWindows|:
// 1. If all window hasn't change their states, the following calls are noops.
// 2. If some windows are manually unminimized, the following call will minimize
// all the unminimized windows.
TEST_F(WallpaperPrivateApiUnittest, HideAndManualUnminimizeWindows) {
  fake_user_manager()->AddUser(kTestAccount1);
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  scoped_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));

  ash::wm::WindowState* window0_state = ash::wm::GetWindowState(window0.get());
  ash::wm::WindowState* window1_state = ash::wm::GetWindowState(window1.get());

  // We then activate window 0 (i.e. wallpaper picker) and call the minimize
  // function.
  window0_state->Activate();
  EXPECT_TRUE(window0_state->IsActive());
  scoped_refptr<TestMinimizeFunction> minimize_function_0(
      new TestMinimizeFunction());
  EXPECT_TRUE(minimize_function_0->RunAsync());

  // All windows except window 0 should be minimized.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Calls minimize function again should be an noop if window state didn't
  // change.
  scoped_refptr<TestMinimizeFunction> minimize_function_1(
      new TestMinimizeFunction());
  EXPECT_TRUE(minimize_function_1->RunAsync());

  // All windows except window 0 should be minimized.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Manually unminimize window 1.
  window1_state->Unminimize();
  EXPECT_FALSE(window1_state->IsMinimized());
  window0_state->Activate();

  scoped_refptr<TestMinimizeFunction> minimize_function_2(
      new TestMinimizeFunction());
  EXPECT_TRUE(minimize_function_2->RunAsync());

  // Window 1 should be minimized again.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Then we destroy window 0 and call the restore function.
  window0.reset();
  scoped_refptr<TestRestoreFunction> restore_function(
      new TestRestoreFunction());
  EXPECT_TRUE(restore_function->RunAsync());

  // Windows 1 should no longer be minimized.
  EXPECT_FALSE(window1_state->IsMinimized());
}

class WallpaperPrivateApiMultiUserUnittest
    : public WallpaperPrivateApiUnittest {
 public:
  WallpaperPrivateApiMultiUserUnittest()
      : multi_user_window_manager_(NULL),
        session_state_delegate_(NULL) {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  void SetUpMultiUserWindowManager(
      const std::string& active_user_id,
      chrome::MultiUserWindowManager::MultiProfileMode mode);

  void SwitchActiveUser(const std::string& active_user_id);

  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager() {
    return multi_user_window_manager_;
  }

 private:
  chrome::MultiUserWindowManagerChromeOS* multi_user_window_manager_;
  ash::test::TestSessionStateDelegate* session_state_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperPrivateApiMultiUserUnittest);
};

void WallpaperPrivateApiMultiUserUnittest::SetUp() {
  AshTestBase::SetUp();
  session_state_delegate_ =
      static_cast<ash::test::TestSessionStateDelegate*> (
          ash::Shell::GetInstance()->session_state_delegate());
  fake_user_manager()->AddUser(kTestAccount1);
  fake_user_manager()->AddUser(kTestAccount2);
}

void WallpaperPrivateApiMultiUserUnittest::TearDown() {
  chrome::MultiUserWindowManager::DeleteInstance();
  AshTestBase::TearDown();
}

void WallpaperPrivateApiMultiUserUnittest::SetUpMultiUserWindowManager(
    const std::string& active_user_id,
    chrome::MultiUserWindowManager::MultiProfileMode mode) {
  multi_user_window_manager_ =
      new chrome::MultiUserWindowManagerChromeOS(active_user_id);
  chrome::MultiUserWindowManager::SetInstanceForTest(
      multi_user_window_manager_, mode);
  // We do not want animations while the test is going on.
  multi_user_window_manager_->SetAnimationSpeedForTest(
      chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_DISABLED);
  EXPECT_TRUE(multi_user_window_manager_);
}

void WallpaperPrivateApiMultiUserUnittest::SwitchActiveUser(
    const std::string& active_user_id) {
  fake_user_manager()->SwitchActiveUser(active_user_id);
  multi_user_window_manager_->ActiveUserChanged(active_user_id);
}

// In multi profile mode, user may open wallpaper picker in one profile and
// then switch to a different profile and open another wallpaper picker
// without closing the first one.
TEST_F(WallpaperPrivateApiMultiUserUnittest, HideAndRestoreWindowsTwoUsers) {
  SetUpMultiUserWindowManager(kTestAccount1,
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_SEPARATED);

  scoped_ptr<aura::Window> window4(CreateTestWindowInShellWithId(4));
  scoped_ptr<aura::Window> window3(CreateTestWindowInShellWithId(3));
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  scoped_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));

  ash::wm::WindowState* window0_state = ash::wm::GetWindowState(window0.get());
  ash::wm::WindowState* window1_state = ash::wm::GetWindowState(window1.get());
  ash::wm::WindowState* window2_state = ash::wm::GetWindowState(window2.get());
  ash::wm::WindowState* window3_state = ash::wm::GetWindowState(window3.get());
  ash::wm::WindowState* window4_state = ash::wm::GetWindowState(window4.get());

  multi_user_window_manager()->SetWindowOwner(window0.get(), kTestAccount1);
  multi_user_window_manager()->SetWindowOwner(window1.get(), kTestAccount1);

  // Set some windows to an inactive owner.
  multi_user_window_manager()->SetWindowOwner(window2.get(), kTestAccount2);
  multi_user_window_manager()->SetWindowOwner(window3.get(), kTestAccount2);
  multi_user_window_manager()->SetWindowOwner(window4.get(), kTestAccount2);

  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_FALSE(window3_state->IsMinimized());
  EXPECT_FALSE(window4_state->IsMinimized());

  // We then activate window 0 (i.e. wallpaper picker) and call the minimize
  // function.
  window0_state->Activate();
  EXPECT_TRUE(window0_state->IsActive());
  scoped_refptr<TestMinimizeFunction> minimize_function_0(
      new TestMinimizeFunction());
  EXPECT_TRUE(minimize_function_0->RunAsync());

  // All windows except window 0 should be minimized.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // All windows that belong to inactive user should not be affected.
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_FALSE(window3_state->IsMinimized());
  EXPECT_FALSE(window4_state->IsMinimized());

  // Activate kTestAccount2. kTestAccount1 becomes inactive user.
  SwitchActiveUser(kTestAccount2);

  window2_state->Activate();
  EXPECT_TRUE(window2_state->IsActive());
  scoped_refptr<TestMinimizeFunction> minimize_function_1(
      new TestMinimizeFunction());
  EXPECT_TRUE(minimize_function_1->RunAsync());

  // All windows except window 2 should be minimized.
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_TRUE(window3_state->IsMinimized());
  EXPECT_TRUE(window4_state->IsMinimized());

  // All windows that belong to inactive user should not be affected.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Destroy window 4. Nothing should happen to other windows.
  window4_state->Unminimize();
  window4.reset();

  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_TRUE(window3_state->IsMinimized());
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Then we destroy window 2 and call the restore function.
  window2.reset();
  scoped_refptr<TestRestoreFunction> restore_function_0(
      new TestRestoreFunction());
  EXPECT_TRUE(restore_function_0->RunAsync());

  EXPECT_FALSE(window3_state->IsMinimized());

  // All windows that belong to inactive user should not be affected.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  SwitchActiveUser(kTestAccount1);

  // Then we destroy window 0 and call the restore function.
  window0.reset();
  scoped_refptr<TestRestoreFunction> restore_function_1(
      new TestRestoreFunction());
  EXPECT_TRUE(restore_function_1->RunAsync());

  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_FALSE(window3_state->IsMinimized());
}

// In multi profile mode, user may teleport windows. Teleported window should
// also be minimized when open wallpaper picker.
TEST_F(WallpaperPrivateApiMultiUserUnittest, HideTeleportedWindow) {
  SetUpMultiUserWindowManager(kTestAccount1,
      chrome::MultiUserWindowManager::MULTI_PROFILE_MODE_MIXED);

  scoped_ptr<aura::Window> window3(CreateTestWindowInShellWithId(3));
  scoped_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  scoped_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  scoped_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));

  ash::wm::WindowState* window0_state = ash::wm::GetWindowState(window0.get());
  ash::wm::WindowState* window1_state = ash::wm::GetWindowState(window1.get());
  ash::wm::WindowState* window2_state = ash::wm::GetWindowState(window2.get());
  ash::wm::WindowState* window3_state = ash::wm::GetWindowState(window3.get());

  multi_user_window_manager()->SetWindowOwner(window0.get(), kTestAccount1);
  multi_user_window_manager()->SetWindowOwner(window1.get(), kTestAccount1);

  // Set some windows to an inactive owner.
  multi_user_window_manager()->SetWindowOwner(window2.get(), kTestAccount2);
  multi_user_window_manager()->SetWindowOwner(window3.get(), kTestAccount2);

  // Teleport window2 to kTestAccount1.
  multi_user_window_manager()->ShowWindowForUser(window2.get(), kTestAccount1);

  // Initial window state. All windows shouldn't be minimized.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_FALSE(window3_state->IsMinimized());

  // We then activate window 0 (i.e. wallpaper picker) and call the minimize
  // function.
  window0_state->Activate();
  EXPECT_TRUE(window0_state->IsActive());
  scoped_refptr<TestMinimizeFunction> minimize_function_0(
      new TestMinimizeFunction());
  EXPECT_TRUE(minimize_function_0->RunAsync());

  // All windows except window 0 should be minimized.
  EXPECT_FALSE(window0_state->IsMinimized());
  EXPECT_TRUE(window1_state->IsMinimized());

  // Teleported window should also be minimized.
  EXPECT_TRUE(window2_state->IsMinimized());
  // Other window should remain the same.
  EXPECT_FALSE(window3_state->IsMinimized());

  // Then we destroy window 0 and call the restore function.
  window0.reset();
  scoped_refptr<TestRestoreFunction> restore_function_1(
      new TestRestoreFunction());
  EXPECT_TRUE(restore_function_1->RunAsync());

  EXPECT_FALSE(window1_state->IsMinimized());
  EXPECT_FALSE(window2_state->IsMinimized());
  EXPECT_FALSE(window3_state->IsMinimized());
}

}  // namespace chromeos
