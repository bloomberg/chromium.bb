// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_action_handler_layout_manager.h"

#include <memory>
#include <utility>

#include "ash/lock_screen_action/lock_screen_action_background_controller.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller_stub.h"
#include "ash/lock_screen_action/test_lock_screen_action_background_controller.h"
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
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
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
  views::Widget* widget_ = nullptr;

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

    action_background_controller_factory_ = base::Bind(
        &LockActionHandlerLayoutManagerTest::CreateActionBackgroundController,
        base::Unretained(this));
    LockScreenActionBackgroundController::SetFactoryCallbackForTesting(
        &action_background_controller_factory_);

    AshTestBase::SetUp();

    views::Widget::InitParams widget_params(
        views::Widget::InitParams::TYPE_WINDOW);
    widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
    lock_window_ =
        CreateTestingWindow(widget_params, kShellWindowId_LockScreenContainer,
                            std::make_unique<TestWindowDelegate>());
  }

  void TearDown() override {
    Shell::GetPrimaryRootWindowController()->DeactivateKeyboard(
        keyboard::KeyboardController::GetInstance());
    lock_window_.reset();
    AshTestBase::TearDown();
    LockScreenActionBackgroundController::SetFactoryCallbackForTesting(nullptr);
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

  void SetUpTrayActionClientAndLockSession(mojom::TrayActionState state) {
    Shell::Get()->tray_action()->SetClient(
        tray_action_client_.CreateInterfacePtrAndBind(), state);
    GetSessionControllerClient()->SetSessionState(
        session_manager::SessionState::LOCKED);
  }

  // Virtual so test specializations can override the background controller
  // implementation used in tests.
  virtual std::unique_ptr<LockScreenActionBackgroundController>
  CreateActionBackgroundController() {
    return std::make_unique<LockScreenActionBackgroundControllerStub>();
  }

 private:
  std::unique_ptr<aura::Window> lock_window_;

  LockScreenActionBackgroundController::FactoryCallback
      action_background_controller_factory_;

  TestTrayActionClient tray_action_client_;

  DISALLOW_COPY_AND_ASSIGN(LockActionHandlerLayoutManagerTest);
};

class LockActionHandlerLayoutManagerTestWithTestBackgroundController
    : public LockActionHandlerLayoutManagerTest {
 public:
  LockActionHandlerLayoutManagerTestWithTestBackgroundController() = default;
  ~LockActionHandlerLayoutManagerTestWithTestBackgroundController() override =
      default;

  void TearDown() override {
    LockActionHandlerLayoutManagerTest::TearDown();
    background_controller_ = nullptr;
  }

  std::unique_ptr<LockScreenActionBackgroundController>
  CreateActionBackgroundController() override {
    auto result = std::make_unique<TestLockScreenActionBackgroundController>();
    EXPECT_FALSE(background_controller_);
    background_controller_ = result.get();
    return result;
  }

  TestLockScreenActionBackgroundController* background_controller() {
    return background_controller_;
  }

 private:
  // The lock screen action background controller created by
  // |CreateActionBackgroundController|.
  TestLockScreenActionBackgroundController* background_controller_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(
      LockActionHandlerLayoutManagerTestWithTestBackgroundController);
};

TEST_F(LockActionHandlerLayoutManagerTest, PreserveNormalWindowBounds) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kActive);

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
      screen_util::GetDisplayWorkAreaBoundsInParent(window.get());
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
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

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
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

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

  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  gfx::Rect target_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  target_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                      kShelfSize /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockActionHandlerLayoutManagerTest, KeyboardBounds) {
  gfx::Rect initial_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  initial_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                       kShelfSize /* bottom */);

  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());
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

  const int accessibility_panel_height = 45;
  shelf_layout_manager->SetAccessibilityPanelHeight(accessibility_panel_height);

  target_bounds.Inset(0 /* left */, accessibility_panel_height /* top */,
                      0 /* right */, 0 /* bottom */);
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());

  ShowKeyboard(false);
}

TEST_F(LockActionHandlerLayoutManagerTest, AddingWindowInActiveState) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest, AddingWindowInLaunchingState) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kLaunching);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest, AddingWindowInNonActiveState) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kAvailable);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  // The window should not be visible if the note action is not in active state.
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());

  // When the action state changes back to active, the window should be
  // shown.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());

  // The window should be hidden again upon leaving the active state.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kAvailable);
  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest, FocusWindowWhileInNonActiveState) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kAvailable);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());

  window->Focus();

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest,
       RequestShowWindowOutsideActiveState) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kAvailable);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_MAXIMIZED;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      nullptr /* window_delegate */);

  EXPECT_EQ(GetContainer(kShellWindowId_LockActionHandlerContainer),
            window->parent());
  EXPECT_FALSE(window->IsVisible());

  window->Show();

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTest, MultipleMonitors) {
  UpdateDisplay("300x400,400x500");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kActive);

  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.show_state = ui::SHOW_STATE_FULLSCREEN;
  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      widget_params, kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

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
  target_bounds.Offset(300, 0);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(target_bounds.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(LockActionHandlerLayoutManagerTestWithTestBackgroundController,
       WindowAddedWhileBackgroundShowing) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kLaunching);

  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  EXPECT_EQ(LockScreenActionBackgroundState::kShowing,
            background_controller()->state());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());

  // Move action to active - this will make the app window showable. Though,
  // showing the window should be delayed until the background finishes
  // showing.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);

  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());

  // Finish showing the background - this should make the app window visible.
  ASSERT_TRUE(background_controller()->FinishShow());

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());

  // Move action state back to available - this should hide the app window, and
  // request the background window to be hidden.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kAvailable);

  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());

  ASSERT_TRUE(background_controller()->FinishHide());

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(background_controller()->GetWindow()->IsVisible());
}

TEST_F(LockActionHandlerLayoutManagerTestWithTestBackgroundController,
       WindowAddedWhenBackgroundShown) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kLaunching);

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  // Finish showing the background - this should make the app window visible
  // once it's created.
  ASSERT_TRUE(background_controller()->FinishShow());

  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());
  EXPECT_EQ(LockScreenActionBackgroundState::kShown,
            background_controller()->state());

  // Move action state back to not available - this should immediately hide both
  // the app and background window,
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(background_controller()->GetWindow()->IsVisible());
  EXPECT_EQ(LockScreenActionBackgroundState::kHidden,
            background_controller()->state());
}

TEST_F(LockActionHandlerLayoutManagerTestWithTestBackgroundController,
       SecondWindowAddedWhileShowingBackground) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kLaunching);

  EXPECT_EQ(LockScreenActionBackgroundState::kShowing,
            background_controller()->state());

  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());

  std::unique_ptr<aura::Window> second_window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(second_window->IsVisible());
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());

  // Finish showing the background. The app windows should be shown at this
  // point.
  ASSERT_TRUE(background_controller()->FinishShow());

  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(window->HasFocus());
  EXPECT_TRUE(second_window->IsVisible());
  EXPECT_TRUE(second_window->HasFocus());
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());

  // Deactivate the action - the windows should get hidden.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kAvailable);

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(second_window->IsVisible());
  EXPECT_EQ(LockScreenActionBackgroundState::kHiding,
            background_controller()->state());
}

TEST_F(LockActionHandlerLayoutManagerTestWithTestBackgroundController,
       RelaunchWhileHidingBackground) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kLaunching);

  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  ASSERT_TRUE(background_controller()->FinishShow());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kAvailable);
  ASSERT_EQ(LockScreenActionBackgroundState::kHiding,
            background_controller()->state());

  // Create new app window to show.
  window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kActive);
  ASSERT_EQ(LockScreenActionBackgroundState::kShowing,
            background_controller()->state());

  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());
  EXPECT_FALSE(window->IsVisible());

  background_controller()->FinishShow();

  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window->HasFocus());
}

TEST_F(LockActionHandlerLayoutManagerTestWithTestBackgroundController,
       ActionDeactivatedWhileShowingTheBackground) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kLaunching);

  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  // Lock screen note action was launched, so the background window is expected
  // to start being shown.
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(LockScreenActionBackgroundState::kShowing,
            background_controller()->state());

  // Move lock screen note action back to available state (i.e. not activated
  // state), and test that the background window starts hiding.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kAvailable);

  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(LockScreenActionBackgroundState::kHiding,
            background_controller()->state());
}

TEST_F(LockActionHandlerLayoutManagerTestWithTestBackgroundController,
       ActionDisabledWhileShowingTheBackground) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kLaunching);

  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  // Lock screen note action was launched, so the background window is expected
  // to start being shown.
  EXPECT_TRUE(background_controller()->GetWindow()->IsVisible());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(LockScreenActionBackgroundState::kShowing,
            background_controller()->state());

  // Make lock screen note action unavailable, and test that the background
  // window is hidden immediately.
  Shell::Get()->tray_action()->UpdateLockScreenNoteState(
      mojom::TrayActionState::kNotAvailable);
  EXPECT_FALSE(background_controller()->GetWindow()->IsVisible());
  EXPECT_FALSE(window->IsVisible());
}

TEST_F(LockActionHandlerLayoutManagerTestWithTestBackgroundController,
       BackgroundWindowBounds) {
  SetUpTrayActionClientAndLockSession(mojom::TrayActionState::kActive);
  ASSERT_TRUE(background_controller()->FinishShow());

  std::unique_ptr<aura::Window> window = CreateTestingWindow(
      views::Widget::InitParams(views::Widget::InitParams::TYPE_WINDOW),
      kShellWindowId_LockActionHandlerContainer,
      std::make_unique<TestWindowDelegate>());

  ASSERT_TRUE(window->IsVisible());
  ASSERT_TRUE(background_controller()->GetWindow()->IsVisible());

  // Verify that the window bounds are equal to work area for the bottom shelf
  // alignment, which matches how the shelf is aligned on the lock screen,
  gfx::Rect target_app_window_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  target_app_window_bounds.Inset(0 /* left */, 0 /* top */, 0 /* right */,
                                 kShelfSize /* bottom */);
  EXPECT_EQ(target_app_window_bounds, window->GetBoundsInScreen());

  EXPECT_EQ(display::Screen::GetScreen()->GetPrimaryDisplay().bounds(),
            background_controller()->GetWindow()->GetBoundsInScreen());
}

}  // namespace ash
