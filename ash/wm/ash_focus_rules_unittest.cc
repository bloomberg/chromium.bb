// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_session_controller_client.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

namespace {

// Defines a |SessionControllerClient| that is used to create and destroy the
// test lock screen widget.
class LockScreenSessionControllerClient : public TestSessionControllerClient {
 public:
  explicit LockScreenSessionControllerClient(SessionController* controller)
      : TestSessionControllerClient(controller) {
    InitializeAndBind();
    CreatePredefinedUserSessions(1);
  }
  ~LockScreenSessionControllerClient() override {}

  // TestSessionControllerClient:
  void RequestLockScreen() override {
    TestSessionControllerClient::RequestLockScreen();
    CreateLockScreen();
    Shell::Get()->UpdateShelfVisibility();
  }

  void UnlockScreen() override {
    TestSessionControllerClient::UnlockScreen();
    if (lock_screen_widget_.get()) {
      lock_screen_widget_->Close();
      lock_screen_widget_.reset(nullptr);
    }

    Shell::Get()->UpdateShelfVisibility();
  }

 private:
  void CreateLockScreen() {
    views::View* lock_view = new views::View;
    lock_screen_widget_.reset(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    gfx::Size ps = lock_view->GetPreferredSize();

    gfx::Size root_window_size = Shell::GetPrimaryRootWindow()->bounds().size();
    params.bounds = gfx::Rect((root_window_size.width() - ps.width()) / 2,
                              (root_window_size.height() - ps.height()) / 2,
                              ps.width(), ps.height());
    params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                        kShellWindowId_LockScreenContainer);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    lock_screen_widget_->Init(params);
    lock_screen_widget_->SetContentsView(lock_view);
    lock_screen_widget_->Show();
    lock_screen_widget_->GetNativeView()->SetName("LockView");
    lock_screen_widget_->GetNativeView()->Focus();
  }

  std::unique_ptr<views::Widget> lock_screen_widget_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenSessionControllerClient);
};

////////////////////////////////////////////////////////////////////////////////

// Defines a class that will be used to test the correct behavior of
// |AshFocusRules| when locking and unlocking the screen.
class LockScreenAshFocusRulesTest : public AshTestBase {
 public:
  LockScreenAshFocusRulesTest() {}
  ~LockScreenAshFocusRulesTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    ash_test_helper()->set_test_session_controller_client(
        base::MakeUnique<LockScreenSessionControllerClient>(
            Shell::Get()->session_controller()));
  }

  void TearDown() override { AshTestBase::TearDown(); }

  aura::Window* CreateWindowInDefaultContainer() {
    return CreateWindowInContainer(kShellWindowId_DefaultContainer);
  }

  aura::Window* CreateWindowInAlwaysOnTopContainer() {
    aura::Window* window =
        CreateWindowInContainer(kShellWindowId_AlwaysOnTopContainer);
    window->SetProperty(aura::client::kAlwaysOnTopKey, true);
    return window;
  }

 private:
  aura::Window* CreateWindowInContainer(int container_id) {
    aura::Window* root_window = Shell::GetPrimaryRootWindow();
    aura::Window* container = Shell::GetContainer(root_window, container_id);
    aura::Window* window = new aura::Window(nullptr);
    window->set_id(0);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->Show();

    aura::client::ParentWindowWithContext(window, container,
                                          gfx::Rect(0, 0, 400, 400));

    window->SetProperty(aura::client::kResizeBehaviorKey,
                        ui::mojom::kResizeBehaviorCanMaximize |
                            ui::mojom::kResizeBehaviorCanMinimize |
                            ui::mojom::kResizeBehaviorCanResize);
    return window;
  }

  std::unique_ptr<LockScreenSessionControllerClient> session_controller_client_;

  DISALLOW_COPY_AND_ASSIGN(LockScreenAshFocusRulesTest);
};

}  // namespace

// Verifies focus is returned (after unlocking the screen) to the most recent
// window that had it before locking the screen.
TEST_F(LockScreenAshFocusRulesTest, RegainFocusAfterUnlock) {
  std::unique_ptr<aura::Window> normal_window(CreateWindowInDefaultContainer());
  std::unique_ptr<aura::Window> always_on_top_window(
      CreateWindowInAlwaysOnTopContainer());

  wm::ActivateWindow(always_on_top_window.get());
  wm::ActivateWindow(normal_window.get());

  EXPECT_TRUE(wm::IsActiveWindow(normal_window.get()));
  EXPECT_TRUE(normal_window->IsVisible());
  EXPECT_TRUE(always_on_top_window->IsVisible());
  EXPECT_TRUE(normal_window->HasFocus());
  EXPECT_FALSE(always_on_top_window->HasFocus());

  wm::WindowState* normal_window_state =
      wm::GetWindowState(normal_window.get());
  wm::WindowState* always_on_top_window_state =
      wm::GetWindowState(always_on_top_window.get());

  EXPECT_TRUE(normal_window_state->CanActivate());
  EXPECT_TRUE(always_on_top_window_state->CanActivate());

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);

  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());
  EXPECT_FALSE(normal_window->HasFocus());
  EXPECT_FALSE(always_on_top_window->HasFocus());
  EXPECT_FALSE(normal_window_state->IsMinimized());
  EXPECT_FALSE(always_on_top_window_state->IsMinimized());
  EXPECT_FALSE(normal_window_state->CanActivate());
  EXPECT_FALSE(always_on_top_window_state->CanActivate());

  UnblockUserSession();

  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  EXPECT_FALSE(normal_window_state->IsMinimized());
  EXPECT_FALSE(always_on_top_window_state->IsMinimized());
  EXPECT_TRUE(normal_window_state->CanActivate());
  EXPECT_TRUE(always_on_top_window_state->CanActivate());
  EXPECT_FALSE(always_on_top_window->HasFocus());
  EXPECT_TRUE(normal_window->HasFocus());
}

// Tests that if a widget has a view which should be initially focused, this
// view doesn't get focused if the widget shows behind the lock screen.
TEST_F(LockScreenAshFocusRulesTest, PreventFocusChangeWithLockScreenPresent) {
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(Shell::Get()->session_controller()->IsScreenLocked());

  views::test::TestInitialFocusWidgetDelegate delegate(CurrentContext());
  EXPECT_FALSE(delegate.view()->HasFocus());
  delegate.GetWidget()->Show();
  EXPECT_FALSE(delegate.GetWidget()->IsActive());
  EXPECT_FALSE(delegate.view()->HasFocus());

  UnblockUserSession();
  EXPECT_FALSE(Shell::Get()->session_controller()->IsScreenLocked());
  EXPECT_TRUE(delegate.GetWidget()->IsActive());
  EXPECT_TRUE(delegate.view()->HasFocus());
}

}  // namespace test
}  // namespace ash
