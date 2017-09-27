// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include <string>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/command_line.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_test_util.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

WorkspaceLayoutManager* GetWorkspaceLayoutManager(aura::Window* container) {
  return static_cast<WorkspaceLayoutManager*>(container->layout_manager());
}

}  // namespace

class WorkspaceLayoutManagerKeyboardTest2 : public AshTestBase {
 public:
  WorkspaceLayoutManagerKeyboardTest2() : layout_manager_(nullptr) {}
  ~WorkspaceLayoutManagerKeyboardTest2() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("800x600");
    aura::Window* default_container =
        Shell::GetPrimaryRootWindowController()->GetContainer(
            kShellWindowId_DefaultContainer);
    layout_manager_ = GetWorkspaceLayoutManager(default_container);
  }

  void ShowKeyboard() {
    layout_manager_->OnKeyboardBoundsChanging(keyboard_bounds_);
    restore_work_area_insets_ =
        display::Screen::GetScreen()->GetPrimaryDisplay().GetWorkAreaInsets();
    Shell::Get()->SetDisplayWorkAreaInsets(
        Shell::GetPrimaryRootWindow(),
        gfx::Insets(0, 0, keyboard_bounds_.height(), 0));
  }

  void HideKeyboard() {
    Shell::Get()->SetDisplayWorkAreaInsets(Shell::GetPrimaryRootWindow(),
                                           restore_work_area_insets_);
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

  const gfx::Rect& keyboard_bounds() const { return keyboard_bounds_; }

 private:
  gfx::Insets restore_work_area_insets_;
  gfx::Rect keyboard_bounds_;
  WorkspaceLayoutManager* layout_manager_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceLayoutManagerKeyboardTest2);
};

// When kAshUseNewVKWindowBehavior flag enabled, do not change accessibility
// keyboard work area in non-sticky mode.
TEST_F(WorkspaceLayoutManagerKeyboardTest2,
       IgnoreWorkAreaChangeinNonStickyMode) {
  keyboard::SetAccessibilityKeyboardEnabled(true);
  InitKeyboardBounds();
  Shell::Get()->CreateKeyboard();
  keyboard::KeyboardController* kb_controller =
      keyboard::KeyboardController::GetInstance();

  gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  gfx::Rect orig_window_bounds(0, 100, work_area.width(),
                               work_area.height() - 100);
  std::unique_ptr<aura::Window> window(
      CreateToplevelTestWindow(orig_window_bounds));

  wm::ActivateWindow(window.get());
  EXPECT_EQ(orig_window_bounds, window->bounds());

  // Open keyboard in non-sticky mode.
  kb_controller->ShowKeyboard(false);
  kb_controller->ui()->GetContentsWindow()->SetBounds(
      keyboard::KeyboardBoundsFromRootBounds(
          Shell::GetPrimaryRootWindow()->bounds(), 100));

  // Window should not be shifted up.
  EXPECT_EQ(orig_window_bounds, window->bounds());

  kb_controller->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_AUTOMATIC);
  EXPECT_EQ(orig_window_bounds, window->bounds());

  // Open keyboard in sticky mode.
  kb_controller->ShowKeyboard(true);

  int shift =
      work_area.height() - kb_controller->GetContainerWindow()->bounds().y();
  gfx::Rect changed_window_bounds(orig_window_bounds);
  changed_window_bounds.Offset(0, -shift);
  // Window should be shifted up.
  EXPECT_EQ(changed_window_bounds, window->bounds());

  kb_controller->HideKeyboard(
      keyboard::KeyboardController::HIDE_REASON_AUTOMATIC);
  EXPECT_EQ(orig_window_bounds, window->bounds());
}

}  // namespace ash
