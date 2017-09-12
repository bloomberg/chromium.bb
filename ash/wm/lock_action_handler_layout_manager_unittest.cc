// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_action_handler_layout_manager.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/interfaces/tray_action.mojom.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/tray_action/test_tray_action_client.h"
#include "ash/tray_action/tray_action.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"
#include "ui/keyboard/keyboard_test_util.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

constexpr int kVirtualKeyboardHeight = 100;

aura::Window* GetContainer(ShellWindowId container_id) {
  return Shell::GetPrimaryRootWindowController()->GetContainer(container_id);
}

class TestWindowDelegate : public views::WidgetDelegate {
 public:
  TestWindowDelegate() = default;
  ~TestWindowDelegate() override = default;

  // views::WidgetDelegate:
  void DeleteDelegate() override { delete this; }
  views::Widget* GetWidget() override { return widget_; }
  const views::Widget* GetWidget() const override { return widget_; }
  bool CanActivate() const override { return true; }
  bool CanResize() const override { return true; }
  bool CanMaximize() const override { return true; }
  bool ShouldAdvanceFocusToTopLevelWidget() const override { return true; }

  void set_widget(views::Widget* widget) { widget_ = widget; }

 private:
  views::Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(TestWindowDelegate);
};

}  // namespace

class LockActionHandlerLayoutManagerTest : public AshTestBase {
 public:
  LockActionHandlerLayoutManagerTest() = default;
  ~LockActionHandlerLayoutManagerTest() override = default;

  void SetUp() override {
    // Allow a virtual keyboard (and initialize it per default).
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        keyboard::switches::kEnableVirtualKeyboard);
    AshTestBase::SetUp();

    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW);
    widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
    lock_window_ =
        CreateTestingWindow(widget_params, kShellWindowId_LockScreenContainer,
                            base::MakeUnique<TestWindowDelegate>());
  }

  void TearDown() override {
    Shell::GetPrimaryRootWindowController()->DeactivateKeyboard(
        keyboard::KeyboardController::GetInstance());
    lock_window_.reset();
    AshTestBase::TearDown();
  }

  std::unique_ptr<aura::Window> CreateTestingWindow(
      views::Widget::InitParams params,
      ShellWindowId parent_id,
      std::unique_ptr<TestWindowDelegate> window_delegate) {
    params.parent = GetContainer(parent_id);
    views::Widget* widget = new views::Widget;
    if (window_delegate) {
      window_delegate->set_widget(widget);
      params.delegate = window_delegate.release();
    }
    widget->Init(params);
    widget->Show();
    return base::WrapUnique<aura::Window>(widget->GetNativeView());
  }

  // Show or hide the keyboard.
  void ShowKeyboard(bool show) {
    keyboard::KeyboardController* keyboard =
        keyboard::KeyboardController::GetInstance();
    ASSERT_TRUE(keyboard);
    if (show == keyboard->keyboard_visible())
      return;

    if (show) {
      keyboard->ShowKeyboard(true);
      keyboard->ui()->GetContentsWindow()->SetBounds(
          keyboard::KeyboardBoundsFromRootBounds(
              Shell::GetPrimaryRootWindow()->bounds(), kVirtualKeyboardHeight));
    } else {
      keyboard->HideKeyboard(keyboard::KeyboardController::HIDE_REASON_MANUAL);
    }

    DCHECK_EQ(show, keyboard->keyboard_visible());
  }

 private:
  std::unique_ptr<aura::Window> lock_window_;

  DISALLOW_COPY_AND_ASSIGN(LockActionHandlerLayoutManagerTest);
};

TEST_F(LockActionHandlerLayoutManagerTest, PreserveNormalWindowBounds) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  const gfx::Rect bounds = gfx::Rect(10, 10, 300, 300);
  widget_params.bounds = bounds;
  // Note: default window delegate (used when no widget delegate is set) does
  // not allow the window to be maximized.
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);
  EXPECT_EQ(bounds.ToString(), window->GetBoundsInScreen().ToString());

  gfx::Rect work_area =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(window.get());
  window->SetBounds(work_area);

  EXPECT_EQ(work_area.ToString(), window->GetBoundsInScreen().ToString());

  const gfx::Rect bounds2 = gfx::Rect(100, 100, 200, 200);
  window->SetBounds(bounds2);
  EXPECT_EQ(bounds2.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockActionHandlerLayoutManagerTest, MaximizedWindowBounds) {
  // Cange the shelf alignment before locking the session.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_RIGHT);

  // This should change the shelf alignment to bottom (temporarily for locked
  // state).
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      base::MakeUnique<TestWindowDelegate>());

  // Verify that the window bounds are equal to work area for the bottom shelf
  // alignment, which matches how the shelf is aligned on the lock screen,
  gfx::Rect target_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  target_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                      kShelfSize /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockActionHandlerLayoutManagerTest, FullscreenWindowBounds) {
  // Cange the shelf alignment before locking the session.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_RIGHT);

  // This should change the shelf alignment to bottom (temporarily for locked
  // state).
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      base::MakeUnique<TestWindowDelegate>());

  // Verify that the window bounds are equal to work area for the bottom shelf
  // alignment, which matches how the shelf is aligned on the lock screen,
  gfx::Rect target_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  target_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                      kShelfSize /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockActionHandlerLayoutManagerTest, MaximizeResizableWindow) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      base::MakeUnique<TestWindowDelegate>());

  gfx::Rect target_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  target_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                      kShelfSize /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockActionHandlerLayoutManagerTest, KeyboardBounds) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  gfx::Rect initial_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  initial_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                       kShelfSize /* bottom */);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      base::MakeUnique<TestWindowDelegate>());
  ASSERT_EQ(initial_bounds.ToString(), window->GetBoundsInScreen().ToString());

  ShowKeyboard(true);

  gfx::Rect keyboard_bounds =
      keyboard::KeyboardController::GetInstance()->current_keyboard_bounds();
  // Sanity check that the keyboard intersects woth original window bounds - if
  // this is not true, the window bounds would remain unchanged.
  ASSERT_TRUE(keyboard_bounds.Intersects(initial_bounds));

  gfx::Rect target_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  target_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                      keyboard_bounds.height() /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());

  // Verify that window bounds get updated when Chromevox bounds are shown (so
  // the Chromevox panel does not overlay with the action handler window).
  ash::ShelfLayoutManager* shelf_layout_manager =
      GetPrimaryShelf()->shelf_layout_manager();
  ASSERT_TRUE(shelf_layout_manager);

  const int chromevox_panel_height = 45;
  shelf_layout_manager->SetChromeVoxPanelHeight(chromevox_panel_height);

  target_bounds.Inset(0 /* left */, chromevox_panel_height /* top */,
                      0 /* right */, 0 /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());

  ShowKeyboard(false);
}

TEST_F(LockActionHandlerLayoutManagerTest, AddingWindowInActiveState) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest, ReparentOnTrayActionStateChanges) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  // The window should not be visible if the new note action handler is not
  // active.
  EXPECT_FALSE(window->IsVisible());

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());

  // When the action state changes to background, the window should remain
  // visible, but it should be reparented.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kBackground);
  EXPECT_TRUE(window->IsVisible());

  // When the action state changes back to active, the window should be
  // reparented again.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);

  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_FALSE(window->IsVisible());
}

TEST_F(LockActionHandlerLayoutManagerTest, AddWindowWhileInBackgroundState) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kBackground);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  // The window should be added to the requested container, but it should not
  // be visible while in background state.
  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_FALSE(window->IsVisible());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);

  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest,
       AddSecondaryWindowWhileInBackground) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kBackground);

  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());

  std::unique_ptr<aura::Window> secondary_window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            secondary_window->parent());
  EXPECT_FALSE(secondary_window->IsVisible());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);

  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());
  EXPECT_TRUE(secondary_window->IsVisible());
  EXPECT_TRUE(secondary_window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest, FocusWindowWhileInBackground) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kBackground);

  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());

  window->Focus();

  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest, HideShowWindowWhileInBackground) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kBackground);

  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());

  window->Hide();

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());

  window->Show();

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest, MultipleMonitors) {
  UpdateDisplay("300x400,400x500");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  TestTrayActionClient tray_action_client;
  Shell::Get()->tray_action()->SetClient(
      tray_action_client.CreateInterfacePtrAndBind(),
      mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      base::MakeUnique<TestWindowDelegate>());

  gfx::Rect target_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  target_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                      kShelfSize /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());

  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 30, 40));
  window_state->Maximize();

  // Maximize the window with as the restore bounds is inside 2nd display but
  // lock container windows are always on primary display.
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  target_bounds = gfx::Rect(300, 400);
  target_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                      kShelfSize /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());

  window_state->SetRestoreBoundsInScreen(gfx::Rect(280, 0, 30, 40));
  window_state->Maximize();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[0], window->GetRootWindow());
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());

  window->SetBoundsInScreen(gfx::Rect(0, 0, 30, 40), GetSecondaryDisplay());
  target_bounds = gfx::Rect(400, 500);
  target_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                      kShelfSize /* bottom */);
  target_bounds.Offset(300, 0);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

}  // namespace ash
