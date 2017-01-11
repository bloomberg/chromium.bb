// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/workspace/workspace_layout_manager.h"

#include <string>
#include <utility>

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_layout_manager.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shell_observer.h"
#include "ash/common/test/ash_test.h"
#include "ash/common/wm/fullscreen_window_finder.h"
#include "ash/common/wm/maximize_mode/workspace_backdrop_delegate.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm/wm_event.h"
#include "ash/common/wm/wm_screen_util.h"
#include "ash/common/wm/workspace/workspace_window_resizer.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

WorkspaceLayoutManager* GetWorkspaceLayoutManager(WmWindow* container) {
  return static_cast<WorkspaceLayoutManager*>(container->GetLayoutManager());
}

}  // namespace

// TODO: this is a duplicate of that in ash/common, and should move once
// keyboard is refactored to work in mash.
class WorkspaceLayoutManagerKeyboardTest : public AshTest {
 public:
  WorkspaceLayoutManagerKeyboardTest() : layout_manager_(nullptr) {}
  ~WorkspaceLayoutManagerKeyboardTest() override {}

  void SetUp() override {
    AshTest::SetUp();
    UpdateDisplay("800x600");
    WmWindow* default_container =
        WmShell::Get()->GetPrimaryRootWindowController()->GetWmContainer(
            kShellWindowId_DefaultContainer);
    layout_manager_ = GetWorkspaceLayoutManager(default_container);
  }

  void ShowKeyboard() {
    layout_manager_->OnKeyboardBoundsChanging(keyboard_bounds_);
    restore_work_area_insets_ =
        display::Screen::GetScreen()->GetPrimaryDisplay().GetWorkAreaInsets();
    WmShell::Get()->SetDisplayWorkAreaInsets(
        WmShell::Get()->GetPrimaryRootWindow(),
        gfx::Insets(0, 0, keyboard_bounds_.height(), 0));
  }

  void HideKeyboard() {
    WmShell::Get()->SetDisplayWorkAreaInsets(
        WmShell::Get()->GetPrimaryRootWindow(), restore_work_area_insets_);
    layout_manager_->OnKeyboardBoundsChanging(gfx::Rect());
  }

  // Initializes the keyboard bounds using the bottom half of the work area.
  void InitKeyboardBounds() {
    gfx::Rect work_area(
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
    keyboard_bounds_.SetRect(work_area.x(),
                             work_area.y() + work_area.height() / 2,
                             work_area.width(), work_area.height() / 2);
  }

  void EnableNewVKMode() {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (!command_line->HasSwitch(::switches::kUseNewVirtualKeyboardBehavior))
      command_line->AppendSwitch(::switches::kUseNewVirtualKeyboardBehavior);
  }

  const gfx::Rect& keyboard_bounds() const { return keyboard_bounds_; }

 private:
  gfx::Insets restore_work_area_insets_;
  gfx::Rect keyboard_bounds_;
  WorkspaceLayoutManager* layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerKeyboardTest);
};

TEST_F(WorkspaceLayoutManagerKeyboardTest, ChangeWorkAreaInNonStickyMode) {
  keyboard::SetAccessibilityKeyboardEnabled(true);
  InitKeyboardBounds();
  Shell::GetInstance()->CreateKeyboard();
  keyboard::KeyboardController* kb_controller =
      keyboard::KeyboardController::GetInstance();

  gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  gfx::Rect orig_window_bounds(0, 100, work_area.width(),
                               work_area.height() - 100);
  std::unique_ptr<WindowOwner> window_owner(
      CreateToplevelTestWindow(orig_window_bounds));
  WmWindow* window = window_owner->window();

  window->Activate();
  EXPECT_EQ(orig_window_bounds, window->GetBounds());

  // Open keyboard in non-sticky mode.
  kb_controller->ShowKeyboard(false);
  kb_controller->ui()->GetKeyboardWindow()->SetBounds(
      keyboard::FullWidthKeyboardBoundsFromRootBounds(
          WmShell::Get()->GetPrimaryRootWindow()->GetBounds(), 100));

  int shift =
      work_area.height() - kb_controller->GetContainerWindow()->bounds().y();
  gfx::Rect changed_window_bounds(orig_window_bounds);
  changed_window_bounds.Offset(0, -shift);
  // Window should be shifted up.
  EXPECT_EQ(changed_window_bounds, window->GetBounds());

  kb_controller->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_AUTOMATIC);
  EXPECT_EQ(orig_window_bounds, window->GetBounds());

  // Open keyboard in sticky mode.
  kb_controller->ShowKeyboard(true);

  // Window should be shifted up.
  EXPECT_EQ(changed_window_bounds, window->GetBounds());

  kb_controller->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_AUTOMATIC);
  EXPECT_EQ(orig_window_bounds, window->GetBounds());
}

// When kAshUseNewVKWindowBehavior flag enabled, do not change accessibility
// keyboard work area in non-sticky mode.
TEST_F(WorkspaceLayoutManagerKeyboardTest,
       IgnoreWorkAreaChangeinNonStickyMode) {
  // Append flag to ignore work area change in non-sticky mode.
  EnableNewVKMode();

  keyboard::SetAccessibilityKeyboardEnabled(true);
  InitKeyboardBounds();
  Shell::GetInstance()->CreateKeyboard();
  keyboard::KeyboardController* kb_controller =
      keyboard::KeyboardController::GetInstance();

  gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  gfx::Rect orig_window_bounds(0, 100, work_area.width(),
                               work_area.height() - 100);
  std::unique_ptr<WindowOwner> window_owner(
      CreateToplevelTestWindow(orig_window_bounds));
  WmWindow* window = window_owner->window();

  window->Activate();
  EXPECT_EQ(orig_window_bounds, window->GetBounds());

  // Open keyboard in non-sticky mode.
  kb_controller->ShowKeyboard(false);
  kb_controller->ui()->GetKeyboardWindow()->SetBounds(
      keyboard::FullWidthKeyboardBoundsFromRootBounds(
          WmShell::Get()->GetPrimaryRootWindow()->GetBounds(), 100));

  // Window should not be shifted up.
  EXPECT_EQ(orig_window_bounds, window->GetBounds());

  kb_controller->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_AUTOMATIC);
  EXPECT_EQ(orig_window_bounds, window->GetBounds());

  // Open keyboard in sticky mode.
  kb_controller->ShowKeyboard(true);

  int shift =
      work_area.height() - kb_controller->GetContainerWindow()->bounds().y();
  gfx::Rect changed_window_bounds(orig_window_bounds);
  changed_window_bounds.Offset(0, -shift);
  // Window should be shifted up.
  EXPECT_EQ(changed_window_bounds, window->GetBounds());

  kb_controller->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_AUTOMATIC);
  EXPECT_EQ(orig_window_bounds, window->GetBounds());
}

}  // namespace ash
