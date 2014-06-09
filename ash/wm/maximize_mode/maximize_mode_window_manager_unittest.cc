// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"

#include <string>

#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/switchable_windows.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

namespace ash {

// TODO(skuhne): These tests are failing on Widows because maximized is there
// differently handled. Fix this!
#if !defined(OS_WIN)

class MaximizeModeWindowManagerTest : public test::AshTestBase {
 public:
  MaximizeModeWindowManagerTest() {}
  virtual ~MaximizeModeWindowManagerTest() {}

  // Creates a window which has a fixed size.
  aura::Window* CreateFixedSizeNonMaximizableWindow(ui::wm::WindowType type,
                                                    const gfx::Rect& bounds) {
    return CreateWindowInWatchedContainer(
        type, bounds, gfx::Size(), false, false);
  }

  // Creates a window which can not be maximized, but resized. |max_size|
  // denotes the maximal possible size, if the size is empty, the window has no
  // upper limit. Note: This function will only work with a single root window.
  aura::Window* CreateNonMaximizableWindow(ui::wm::WindowType type,
                                           const gfx::Rect& bounds,
                                           const gfx::Size& max_size) {
    return CreateWindowInWatchedContainer(type, bounds, max_size, false, true);
  }

  // Creates a maximizable and resizable window.
  aura::Window* CreateWindow(ui::wm::WindowType type,
                             const gfx::Rect bounds) {
    return CreateWindowInWatchedContainer(
        type, bounds, gfx::Size(), true, true);
  }

  // Creates a window which also has a widget.
  aura::Window* CreateWindowWithWidget(const gfx::Rect& bounds) {
    views::Widget* widget = new views::Widget();
    views::Widget::InitParams params;
    params.context = CurrentContext();
    // Note: The widget will get deleted with the window.
    widget->Init(params);
    widget->Show();
    aura::Window* window = widget->GetNativeWindow();
    window->SetBounds(bounds);
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);

    return window;
  }

  // Create the Maximized mode window manager.
  ash::MaximizeModeWindowManager* CreateMaximizeModeWindowManager() {
    EXPECT_FALSE(maximize_mode_window_manager());
    Shell::GetInstance()->maximize_mode_controller()->
        EnableMaximizeModeWindowManager(true);
    return maximize_mode_window_manager();
  }

  // Destroy the maximized mode window manager.
  void DestroyMaximizeModeWindowManager() {
    Shell::GetInstance()->maximize_mode_controller()->
        EnableMaximizeModeWindowManager(false);
    EXPECT_FALSE(maximize_mode_window_manager());
  }

  // Get the maximze window manager.
  ash::MaximizeModeWindowManager* maximize_mode_window_manager() {
    return Shell::GetInstance()->maximize_mode_controller()->
        maximize_mode_window_manager_.get();
  }

  // Resize our desktop.
  void ResizeDesktop(int width_delta) {
    gfx::Size size = Shell::GetScreen()->GetDisplayNearestWindow(
        Shell::GetPrimaryRootWindow()).size();
    size.Enlarge(0, width_delta);
    UpdateDisplay(size.ToString());
  }

 private:
  // Create a window in one of the containers which are watched by the
  // MaximizeModeWindowManager. Note that this only works with one root window.
  // If |can_maximize| is not set, |max_size| is the upper limiting size for
  // the window, whereas an empty size means that there is no limit.
  aura::Window* CreateWindowInWatchedContainer(ui::wm::WindowType type,
                                               const gfx::Rect& bounds,
                                               const gfx::Size& max_size,
                                               bool can_maximize,
                                               bool can_resize) {
    aura::test::TestWindowDelegate* delegate = NULL;
    if (!can_maximize) {
      delegate = aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
      delegate->set_window_component(HTCAPTION);
      if (!max_size.IsEmpty())
        delegate->set_maximum_size(max_size);
    }
    aura::Window* window = aura::test::CreateTestWindowWithDelegateAndType(
        delegate, type, 0, bounds, NULL);
    window->SetProperty(aura::client::kCanMaximizeKey, can_maximize);
    if (!can_resize)
      window->SetProperty(aura::client::kCanResizeKey, false);
    aura::Window* container = Shell::GetContainer(
        Shell::GetPrimaryRootWindow(),
        kSwitchableWindowContainerIds[0]);
    container->AddChild(window);
    return window;
  }

  DISALLOW_COPY_AND_ASSIGN(MaximizeModeWindowManagerTest);
};

// Test that creating the object and destroying it without any windows should
// not cause any problems.
TEST_F(MaximizeModeWindowManagerTest, SimpleStart) {
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  DestroyMaximizeModeWindowManager();
}

// Test that existing windows will handled properly when going into maximized
// mode.
TEST_F(MaximizeModeWindowManagerTest, PreCreateWindows) {
  // Bounds for windows we know can be controlled.
  gfx::Rect rect1(10, 10, 200, 50);
  gfx::Rect rect2(10, 60, 200, 50);
  gfx::Rect rect3(20, 140, 100, 100);
  // Bounds for anything else.
  gfx::Rect rect(80, 90, 100, 110);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect1));
  scoped_ptr<aura::Window> w2(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect2));
  scoped_ptr<aura::Window> w3(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect3));
  scoped_ptr<aura::Window> w4(CreateWindow(ui::wm::WINDOW_TYPE_PANEL, rect));
  scoped_ptr<aura::Window> w5(CreateWindow(ui::wm::WINDOW_TYPE_POPUP, rect));
  scoped_ptr<aura::Window> w6(CreateWindow(ui::wm::WINDOW_TYPE_CONTROL, rect));
  scoped_ptr<aura::Window> w7(CreateWindow(ui::wm::WINDOW_TYPE_MENU, rect));
  scoped_ptr<aura::Window> w8(CreateWindow(ui::wm::WINDOW_TYPE_TOOLTIP, rect));
  EXPECT_FALSE(wm::GetWindowState(w1.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w2.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w3.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
  EXPECT_EQ(rect3.ToString(), w3->bounds().ToString());

  // Create the manager and make sure that all qualifying windows were detected
  // and changed.
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(wm::GetWindowState(w1.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(w2.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w3.get())->IsMaximized());
  EXPECT_NE(rect3.origin().ToString(), w3->bounds().origin().ToString());
  EXPECT_EQ(rect3.size().ToString(), w3->bounds().size().ToString());

  // All other windows should not have been touched.
  EXPECT_FALSE(wm::GetWindowState(w4.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w5.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w6.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w7.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w8.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w7->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w8->bounds().ToString());

  // Destroy the manager again and check that the windows return to their
  // previous state.
  DestroyMaximizeModeWindowManager();
  EXPECT_FALSE(wm::GetWindowState(w1.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w2.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w3.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
  EXPECT_EQ(rect3.ToString(), w3->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w7->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w8->bounds().ToString());
}

// Test that non-maximizable windows get properly handled when going into
// maximized mode.
TEST_F(MaximizeModeWindowManagerTest,
       PreCreateNonMaximizableButResizableWindows) {
  // The window bounds.
  gfx::Rect rect(10, 10, 200, 50);
  gfx::Size max_size(300, 200);
  gfx::Size empty_size;
  scoped_ptr<aura::Window> unlimited_window(
      CreateNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect, empty_size));
  scoped_ptr<aura::Window> limited_window(
      CreateNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect, max_size));
  scoped_ptr<aura::Window> fixed_window(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  EXPECT_FALSE(wm::GetWindowState(unlimited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), unlimited_window->bounds().ToString());
  EXPECT_FALSE(wm::GetWindowState(limited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), limited_window->bounds().ToString());
  EXPECT_FALSE(wm::GetWindowState(fixed_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), fixed_window->bounds().ToString());

  gfx::Size workspace_size = ScreenUtil::GetMaximizedWindowBoundsInParent(
      unlimited_window.get()).size();

  // Create the manager and make sure that all qualifying windows were detected
  // and changed.
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  // The unlimited window should have the size of the workspace / parent window.
  EXPECT_FALSE(wm::GetWindowState(unlimited_window.get())->IsMaximized());
  EXPECT_EQ("0,0", unlimited_window->bounds().origin().ToString());
  EXPECT_EQ(workspace_size.ToString(),
            unlimited_window->bounds().size().ToString());
  // The limited window should have the size of the upper possible bounds.
  EXPECT_FALSE(wm::GetWindowState(limited_window.get())->IsMaximized());
  EXPECT_NE(rect.origin().ToString(),
            limited_window->bounds().origin().ToString());
  EXPECT_EQ(max_size.ToString(),
            limited_window->bounds().size().ToString());
  // The fixed size window should have the size of the original window.
  EXPECT_FALSE(wm::GetWindowState(fixed_window.get())->IsMaximized());
  EXPECT_NE(rect.origin().ToString(),
            fixed_window->bounds().origin().ToString());
  EXPECT_EQ(rect.size().ToString(),
            fixed_window->bounds().size().ToString());

  // Destroy the manager again and check that the windows return to their
  // previous state.
  DestroyMaximizeModeWindowManager();
  EXPECT_FALSE(wm::GetWindowState(unlimited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), unlimited_window->bounds().ToString());
  EXPECT_FALSE(wm::GetWindowState(limited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), limited_window->bounds().ToString());
  EXPECT_FALSE(wm::GetWindowState(fixed_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), fixed_window->bounds().ToString());
}

// Test that creating windows while a maximizer exists picks them properly up.
TEST_F(MaximizeModeWindowManagerTest, CreateWindows) {
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());

  // Create the windows and see that the window manager picks them up.
  // Rects for windows we know can be controlled.
  gfx::Rect rect1(10, 10, 200, 50);
  gfx::Rect rect2(10, 60, 200, 50);
  gfx::Rect rect3(20, 140, 100, 100);
  // One rect for anything else.
  gfx::Rect rect(80, 90, 100, 110);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect1));
  scoped_ptr<aura::Window> w2(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect2));
  scoped_ptr<aura::Window> w3(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect3));
  scoped_ptr<aura::Window> w4(CreateWindow(ui::wm::WINDOW_TYPE_PANEL, rect));
  scoped_ptr<aura::Window> w5(CreateWindow(ui::wm::WINDOW_TYPE_POPUP, rect));
  scoped_ptr<aura::Window> w6(CreateWindow(ui::wm::WINDOW_TYPE_CONTROL, rect));
  scoped_ptr<aura::Window> w7(CreateWindow(ui::wm::WINDOW_TYPE_MENU, rect));
  scoped_ptr<aura::Window> w8(CreateWindow(ui::wm::WINDOW_TYPE_TOOLTIP, rect));
  EXPECT_TRUE(wm::GetWindowState(w1.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(w2.get())->IsMaximized());
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  EXPECT_FALSE(wm::GetWindowState(w3.get())->IsMaximized());

  // Make sure that the position of the unresizable window is in the middle of
  // the screen.
  gfx::Size work_area_size =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(w3.get()).size();
  gfx::Point center =
      gfx::Point((work_area_size.width() - rect3.size().width()) / 2,
                 (work_area_size.height() - rect3.size().height()) / 2);
  gfx::Rect centered_window_bounds = gfx::Rect(center, rect3.size());
  EXPECT_EQ(centered_window_bounds.ToString(), w3->bounds().ToString());

  // All other windows should not have been touched.
  EXPECT_FALSE(wm::GetWindowState(w4.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w5.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w6.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w7.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w8.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w7->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w8->bounds().ToString());

  // After the maximize mode was disabled all windows fall back into the mode
  // they were created for.
  DestroyMaximizeModeWindowManager();
  EXPECT_FALSE(wm::GetWindowState(w1.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w2.get())->IsMaximized());
  EXPECT_FALSE(wm::GetWindowState(w3.get())->IsMaximized());
  EXPECT_EQ(rect1.ToString(), w1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), w2->bounds().ToString());
  EXPECT_EQ(rect3.ToString(), w3->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w4->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w5->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w6->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w7->bounds().ToString());
  EXPECT_EQ(rect.ToString(), w8->bounds().ToString());
}

// Test that a window which got created while the maximize mode window manager
// is active gets restored to a usable (non tiny) size upon switching back.
TEST_F(MaximizeModeWindowManagerTest,
       CreateWindowInMaximizedModeRestoresToUsefulSize) {
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());

  // We pass in an empty rectangle to simulate a window creation with no
  // particular size.
  gfx::Rect empty_rect(0, 0, 0, 0);
  scoped_ptr<aura::Window> window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL,
                                               empty_rect));
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());
  EXPECT_NE(empty_rect.ToString(), window->bounds().ToString());
  gfx::Rect maximized_size = window->bounds();

  // Destroy the maximize mode and check that the resulting size of the window
  // is remaining as it is (but not maximized).
  DestroyMaximizeModeWindowManager();

  EXPECT_FALSE(wm::GetWindowState(window.get())->IsMaximized());
  EXPECT_EQ(maximized_size.ToString(), window->bounds().ToString());
}

// Test that non-maximizable windows get properly handled when created in
// maximized mode.
TEST_F(MaximizeModeWindowManagerTest,
       CreateNonMaximizableButResizableWindows) {
  // Create the manager and make sure that all qualifying windows were detected
  // and changed.
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);

  gfx::Rect rect(10, 10, 200, 50);
  gfx::Size max_size(300, 200);
  gfx::Size empty_size;
  scoped_ptr<aura::Window> unlimited_window(
      CreateNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect, empty_size));
  scoped_ptr<aura::Window> limited_window(
      CreateNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect, max_size));
  scoped_ptr<aura::Window> fixed_window(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));

  gfx::Size workspace_size = ScreenUtil::GetMaximizedWindowBoundsInParent(
      unlimited_window.get()).size();

  // All windows should be sized now as big as possible and be centered.
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  // The unlimited window should have the size of the workspace / parent window.
  EXPECT_FALSE(wm::GetWindowState(unlimited_window.get())->IsMaximized());
  EXPECT_EQ("0,0", unlimited_window->bounds().origin().ToString());
  EXPECT_EQ(workspace_size.ToString(),
            unlimited_window->bounds().size().ToString());
  // The limited window should have the size of the upper possible bounds.
  EXPECT_FALSE(wm::GetWindowState(limited_window.get())->IsMaximized());
  EXPECT_NE(rect.origin().ToString(),
            limited_window->bounds().origin().ToString());
  EXPECT_EQ(max_size.ToString(),
            limited_window->bounds().size().ToString());
  // The fixed size window should have the size of the original window.
  EXPECT_FALSE(wm::GetWindowState(fixed_window.get())->IsMaximized());
  EXPECT_NE(rect.origin().ToString(),
            fixed_window->bounds().origin().ToString());
  EXPECT_EQ(rect.size().ToString(),
            fixed_window->bounds().size().ToString());

  // Destroy the manager again and check that the windows return to their
  // creation state.
  DestroyMaximizeModeWindowManager();

  EXPECT_FALSE(wm::GetWindowState(unlimited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), unlimited_window->bounds().ToString());
  EXPECT_FALSE(wm::GetWindowState(limited_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), limited_window->bounds().ToString());
  EXPECT_FALSE(wm::GetWindowState(fixed_window.get())->IsMaximized());
  EXPECT_EQ(rect.ToString(), fixed_window->bounds().ToString());
}

// Create a string which consists of the bounds and the state for comparison.
std::string GetPlacementString(const gfx::Rect& bounds,
                               ui::WindowShowState state) {
  return bounds.ToString() + base::StringPrintf(" %d", state);
}

// Retrieves the window's restore state override - if any - and returns it as a
// string.
std::string GetPlacementOverride(aura::Window* window) {
  gfx::Rect* bounds = window->GetProperty(ash::kRestoreBoundsOverrideKey);
  if (bounds) {
    gfx::Rect restore_bounds = *bounds;
    ui::WindowShowState restore_state =
        window->GetProperty(ash::kRestoreShowStateOverrideKey);
    return GetPlacementString(restore_bounds, restore_state);
  }
  return std::string();
}

// Test that the restore state will be kept at its original value for
// session restauration purposes.
TEST_F(MaximizeModeWindowManagerTest, TestRestoreIntegrety) {
  gfx::Rect bounds(10, 10, 200, 50);
  gfx::Size empty_size;
  gfx::Rect empty_bounds;
  scoped_ptr<aura::Window> normal_window(
      CreateWindowWithWidget(bounds));
  scoped_ptr<aura::Window> maximized_window(
      CreateWindowWithWidget(bounds));
  wm::GetWindowState(maximized_window.get())->Maximize();

  EXPECT_EQ(std::string(), GetPlacementOverride(normal_window.get()));
  EXPECT_EQ(std::string(), GetPlacementOverride(maximized_window.get()));

  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);

  // With the maximization the override states should be returned in its
  // pre-maximized state.
  EXPECT_EQ(GetPlacementString(bounds, ui::SHOW_STATE_NORMAL),
            GetPlacementOverride(normal_window.get()));
  EXPECT_EQ(GetPlacementString(bounds, ui::SHOW_STATE_MAXIMIZED),
            GetPlacementOverride(maximized_window.get()));

  // Changing a window's state now does not change the returned result.
  wm::GetWindowState(maximized_window.get())->Minimize();
  EXPECT_EQ(GetPlacementString(bounds, ui::SHOW_STATE_MAXIMIZED),
            GetPlacementOverride(maximized_window.get()));

  // Destroy the manager again and check that the overrides get reset.
  DestroyMaximizeModeWindowManager();
  EXPECT_EQ(std::string(), GetPlacementOverride(normal_window.get()));
  EXPECT_EQ(std::string(), GetPlacementOverride(maximized_window.get()));

  // Changing a window's state now does not bring the overrides back.
  wm::GetWindowState(maximized_window.get())->Restore();
  gfx::Rect new_bounds(10, 10, 200, 50);
  maximized_window->SetBounds(new_bounds);

  EXPECT_EQ(std::string(), GetPlacementOverride(maximized_window.get()));
}

// Test that windows which got created before the maximizer was created can be
// destroyed while the maximizer is still running.
TEST_F(MaximizeModeWindowManagerTest, PreCreateWindowsDeleteWhileActive) {
  ash::MaximizeModeWindowManager* manager = NULL;
  {
    // Bounds for windows we know can be controlled.
    gfx::Rect rect1(10, 10, 200, 50);
    gfx::Rect rect2(10, 60, 200, 50);
    gfx::Rect rect3(20, 140, 100, 100);
    // Bounds for anything else.
    gfx::Rect rect(80, 90, 100, 110);
    scoped_ptr<aura::Window> w1(
        CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect1));
    scoped_ptr<aura::Window> w2(
        CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect2));
    scoped_ptr<aura::Window> w3(
        CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect3));

    // Create the manager and make sure that all qualifying windows were
    // detected and changed.
    manager = CreateMaximizeModeWindowManager();
    ASSERT_TRUE(manager);
    EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  }
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  DestroyMaximizeModeWindowManager();
}

// Test that windows which got created while the maximizer was running can get
// destroyed before the maximizer gets destroyed.
TEST_F(MaximizeModeWindowManagerTest, CreateWindowsAndDeleteWhileActive) {
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  {
    // Bounds for windows we know can be controlled.
    gfx::Rect rect1(10, 10, 200, 50);
    gfx::Rect rect2(10, 60, 200, 50);
    gfx::Rect rect3(20, 140, 100, 100);
    scoped_ptr<aura::Window> w1(
        CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, gfx::Rect(10, 10, 200, 50)));
    scoped_ptr<aura::Window> w2(
        CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, gfx::Rect(10, 60, 200, 50)));
    scoped_ptr<aura::Window> w3(
        CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL,
                                            gfx::Rect(20, 140, 100, 100)));
    // Check that the windows got automatically maximized as well.
    EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
    EXPECT_TRUE(wm::GetWindowState(w1.get())->IsMaximized());
    EXPECT_TRUE(wm::GetWindowState(w2.get())->IsMaximized());
    EXPECT_FALSE(wm::GetWindowState(w3.get())->IsMaximized());
  }
  EXPECT_EQ(0, manager->GetNumberOfManagedWindows());
  DestroyMaximizeModeWindowManager();
}

// Test that windows which were maximized stay maximized.
TEST_F(MaximizeModeWindowManagerTest, MaximizedShouldRemainMaximized) {
  // Bounds for windows we know can be controlled.
  gfx::Rect rect(10, 10, 200, 50);
  scoped_ptr<aura::Window> window(
      CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::GetWindowState(window.get())->Maximize();

  // Create the manager and make sure that the window gets detected.
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(1, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());

  // Destroy the manager again and check that the window will remain maximized.
  DestroyMaximizeModeWindowManager();
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMaximized());
  wm::GetWindowState(window.get())->Restore();
  EXPECT_EQ(rect.ToString(), window->bounds().ToString());
}

// Test that minimized windows do neither get maximized nor restored upon
// entering maximized mode and get restored to their previous state after
// leaving.
TEST_F(MaximizeModeWindowManagerTest, MinimizedWindowBehavior) {
  // Bounds for windows we know can be controlled.
  gfx::Rect rect(10, 10, 200, 50);
  scoped_ptr<aura::Window> initially_minimized_window(
      CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window> initially_normal_window(
      CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window> initially_maximized_window(
      CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::GetWindowState(initially_minimized_window.get())->Minimize();
  wm::GetWindowState(initially_maximized_window.get())->Maximize();

  // Create the manager and make sure that the window gets detected.
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(3, manager->GetNumberOfManagedWindows());
  EXPECT_TRUE(wm::GetWindowState(
      initially_minimized_window.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(initially_normal_window.get())->IsMaximized());
  EXPECT_TRUE(wm::GetWindowState(
      initially_maximized_window.get())->IsMaximized());
  // Now minimize the second window to check that upon leaving the window
  // will get restored to its minimized state.
  wm::GetWindowState(initially_normal_window.get())->Minimize();
  wm::GetWindowState(initially_maximized_window.get())->Minimize();
  EXPECT_TRUE(wm::GetWindowState(
      initially_minimized_window.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(
      initially_normal_window.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(
      initially_maximized_window.get())->IsMinimized());

  // Destroy the manager again and check that the window will get minimized.
  DestroyMaximizeModeWindowManager();
  EXPECT_TRUE(wm::GetWindowState(
      initially_minimized_window.get())->IsMinimized());
  EXPECT_FALSE(wm::GetWindowState(
                   initially_normal_window.get())->IsMinimized());
  EXPECT_TRUE(wm::GetWindowState(
      initially_maximized_window.get())->IsMaximized());
}

// Check that resizing the desktop does reposition unmaximizable, unresizable &
// managed windows.
TEST_F(MaximizeModeWindowManagerTest, DesktopSizeChangeMovesUnmaximizable) {
  UpdateDisplay("400x400");
  // This window will move because it does not fit the new bounds.
  gfx::Rect rect(20, 300, 100, 100);
  scoped_ptr<aura::Window> window1(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  EXPECT_EQ(rect.ToString(), window1->bounds().ToString());

  // This window will not move because it does fit the new bounds.
  gfx::Rect rect2(20, 140, 100, 100);
  scoped_ptr<aura::Window> window2(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect2));

  // Turning on the manager will reposition (but not resize) the window.
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(2, manager->GetNumberOfManagedWindows());
  gfx::Rect moved_bounds(window1->bounds());
  EXPECT_NE(rect.origin().ToString(), moved_bounds.origin().ToString());
  EXPECT_EQ(rect.size().ToString(), moved_bounds.size().ToString());

  // Simulating a desktop resize should move the window again.
  UpdateDisplay("300x300");
  gfx::Rect new_moved_bounds(window1->bounds());
  EXPECT_NE(rect.origin().ToString(), new_moved_bounds.origin().ToString());
  EXPECT_EQ(rect.size().ToString(), new_moved_bounds.size().ToString());
  EXPECT_NE(moved_bounds.origin().ToString(), new_moved_bounds.ToString());

  // Turning off the mode should not restore to the initial coordinates since
  // the new resolution is smaller and the window was on the edge.
  DestroyMaximizeModeWindowManager();
  EXPECT_NE(rect.ToString(), window1->bounds().ToString());
  EXPECT_EQ(rect2.ToString(), window2->bounds().ToString());
}

// Check that windows return to original location if desktop size changes to
// something else and back while in maximize mode.
TEST_F(MaximizeModeWindowManagerTest, SizeChangeReturnWindowToOriginalPos) {
  gfx::Rect rect(20, 140, 100, 100);
  scoped_ptr<aura::Window> window(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));

  // Turning on the manager will reposition (but not resize) the window.
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(1, manager->GetNumberOfManagedWindows());
  gfx::Rect moved_bounds(window->bounds());
  EXPECT_NE(rect.origin().ToString(), moved_bounds.origin().ToString());
  EXPECT_EQ(rect.size().ToString(), moved_bounds.size().ToString());

  // Simulating a desktop resize should move the window again.
  ResizeDesktop(-10);
  gfx::Rect new_moved_bounds(window->bounds());
  EXPECT_NE(rect.origin().ToString(), new_moved_bounds.origin().ToString());
  EXPECT_EQ(rect.size().ToString(), new_moved_bounds.size().ToString());
  EXPECT_NE(moved_bounds.origin().ToString(), new_moved_bounds.ToString());

  // Then resize back to the original desktop size which should move windows
  // to their original location after leaving the maximize mode.
  ResizeDesktop(10);
  DestroyMaximizeModeWindowManager();
  EXPECT_EQ(rect.ToString(), window->bounds().ToString());
}

// Check that enabling of the maximize mode does not have an impact on the MRU
// order of windows.
TEST_F(MaximizeModeWindowManagerTest, ModeChangeKeepsMRUOrder) {
  gfx::Rect rect(20, 140, 100, 100);
  scoped_ptr<aura::Window> w1(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window> w2(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window> w3(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window> w4(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window> w5(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));

  // The windows should be in the reverse order of creation in the MRU list.
  {
    MruWindowTracker::WindowList windows =
        MruWindowTracker::BuildWindowList(false);
    EXPECT_EQ(w1.get(), windows[4]);
    EXPECT_EQ(w2.get(), windows[3]);
    EXPECT_EQ(w3.get(), windows[2]);
    EXPECT_EQ(w4.get(), windows[1]);
    EXPECT_EQ(w5.get(), windows[0]);
  }

  // Activating the window manager should keep the order.
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_EQ(5, manager->GetNumberOfManagedWindows());
  {
    MruWindowTracker::WindowList windows =
        MruWindowTracker::BuildWindowList(false);
    // We do not test maximization here again since that was done already.
    EXPECT_EQ(w1.get(), windows[4]);
    EXPECT_EQ(w2.get(), windows[3]);
    EXPECT_EQ(w3.get(), windows[2]);
    EXPECT_EQ(w4.get(), windows[1]);
    EXPECT_EQ(w5.get(), windows[0]);
  }

  // Destroying should still keep the order.
  DestroyMaximizeModeWindowManager();
  {
    MruWindowTracker::WindowList windows =
        MruWindowTracker::BuildWindowList(false);
    // We do not test maximization here again since that was done already.
    EXPECT_EQ(w1.get(), windows[4]);
    EXPECT_EQ(w2.get(), windows[3]);
    EXPECT_EQ(w3.get(), windows[2]);
    EXPECT_EQ(w4.get(), windows[1]);
    EXPECT_EQ(w5.get(), windows[0]);
  }
}

// Check that a restore state change does always restore to maximized.
TEST_F(MaximizeModeWindowManagerTest, IgnoreRestoreStateChages) {
  gfx::Rect rect(20, 140, 100, 100);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* window_state = wm::GetWindowState(w1.get());
  CreateMaximizeModeWindowManager();
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->Minimize();
  EXPECT_TRUE(window_state->IsMinimized());
  window_state->Restore();
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->Restore();
  EXPECT_TRUE(window_state->IsMaximized());
  DestroyMaximizeModeWindowManager();
}

// Check that minimize and restore do the right thing.
TEST_F(MaximizeModeWindowManagerTest, TestMinimize) {
  gfx::Rect rect(10, 10, 100, 100);
  scoped_ptr<aura::Window> window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL,
                                  rect));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  EXPECT_EQ(rect.ToString(), window->bounds().ToString());
  ash::Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());

  window_state->Minimize();
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_FALSE(window->IsVisible());

  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());

  ash::Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_TRUE(window->IsVisible());
}

// Check that a full screen window is staying full screen in maximize mode,
// and that it returns to full screen thereafter (if left).
TEST_F(MaximizeModeWindowManagerTest, KeepFullScreenModeOn) {
  gfx::Rect rect(20, 140, 100, 100);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* window_state = wm::GetWindowState(w1.get());

  ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();

  // Allow the shelf to hide.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event);

  // With full screen, the shelf should get hidden.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  CreateMaximizeModeWindowManager();

  // The Full screen mode should continue to be on.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  // With leaving the fullscreen mode, the maximized mode should return and the
  // shelf should become visible.
  window_state->OnWMEvent(&event);
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  // Ending the maximize mode should return to full screen and the shelf should
  // be hidden again.
  DestroyMaximizeModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
}

// Check that full screen mode can be turned on in maximized mode and remains
// upon coming back.
TEST_F(MaximizeModeWindowManagerTest, AllowFullScreenMode) {
  gfx::Rect rect(20, 140, 100, 100);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* window_state = wm::GetWindowState(w1.get());

  ShelfLayoutManager* shelf =
      Shell::GetPrimaryRootWindowController()->GetShelfLayoutManager();

  // Allow the shelf to hide.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->visibility_state());

  CreateMaximizeModeWindowManager();

  // Fullscreen mode should still be off and the shelf should be visible.
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_VISIBLE, shelf->visibility_state());

  // After going into fullscreen mode, the shelf should be hidden.
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event);
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());

  // With the destruction of the manager we should remain in full screen.
  DestroyMaximizeModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(SHELF_HIDDEN, shelf->visibility_state());
}

// Check that the full screen mode will stay active when the maximize mode is
// ended.
TEST_F(MaximizeModeWindowManagerTest,
       FullScreenModeRemainsWhenCreatedInMaximizedMode) {
  CreateMaximizeModeWindowManager();

  gfx::Rect rect(20, 140, 100, 100);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* window_state = wm::GetWindowState(w1.get());
  wm::WMEvent event_full_screen(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event_full_screen);
  EXPECT_TRUE(window_state->IsFullscreen());

  // After the maximize mode manager is ended, full screen will remain.
  DestroyMaximizeModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
}

// Check that the full screen mode will stay active throughout a maximzied mode
// session.
TEST_F(MaximizeModeWindowManagerTest,
       FullScreenModeRemainsThroughMaximizeModeSwitch) {
  gfx::Rect rect(20, 140, 100, 100);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* window_state = wm::GetWindowState(w1.get());
  wm::WMEvent event_full_screen(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event_full_screen);
  EXPECT_TRUE(window_state->IsFullscreen());

  CreateMaximizeModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
  DestroyMaximizeModeWindowManager();
  EXPECT_TRUE(window_state->IsFullscreen());
}

// Check that an empty window does not get restored to a tiny size.
TEST_F(MaximizeModeWindowManagerTest,
       CreateAndMaximizeInMaximizeModeShouldRetoreToGoodSizeGoingToDefault) {
  CreateMaximizeModeWindowManager();
  gfx::Rect rect;
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  w1->Show();
  wm::WindowState* window_state = wm::GetWindowState(w1.get());
  EXPECT_TRUE(window_state->IsMaximized());

  // There is a calling order in which the restore bounds can get set to an
  // empty rectangle. We simulate this here.
  window_state->SetRestoreBoundsInScreen(rect);
  EXPECT_TRUE(window_state->GetRestoreBoundsInScreen().IsEmpty());

  // Setting the window to a new size will physically not change the window,
  // but the restore size should get updated so that a restore later on will
  // return to this size.
  gfx::Rect requested_bounds(10, 20, 50, 70);
  w1->SetBounds(requested_bounds);
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(requested_bounds.ToString(),
            window_state->GetRestoreBoundsInScreen().ToString());

  DestroyMaximizeModeWindowManager();

  EXPECT_FALSE(window_state->IsMaximized());
  EXPECT_EQ(w1->bounds().ToString(), requested_bounds.ToString());
}

// Check that snapping operations get ignored.
TEST_F(MaximizeModeWindowManagerTest, SnapModeTests) {
  gfx::Rect rect(20, 140, 100, 100);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* window_state = wm::GetWindowState(w1.get());
  wm::WMEvent event_left(wm::WM_EVENT_SNAP_LEFT);
  wm::WMEvent event_right(wm::WM_EVENT_SNAP_RIGHT);
  window_state->OnWMEvent(&event_left);
  EXPECT_TRUE(window_state->IsSnapped());

  CreateMaximizeModeWindowManager();

  // Fullscreen mode should now be off and it should not come back while in
  // maximize mode.
  EXPECT_FALSE(window_state->IsSnapped());
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->OnWMEvent(&event_left);
  EXPECT_FALSE(window_state->IsSnapped());
  EXPECT_TRUE(window_state->IsMaximized());
  window_state->OnWMEvent(&event_right);
  EXPECT_FALSE(window_state->IsSnapped());
  EXPECT_TRUE(window_state->IsMaximized());

  DestroyMaximizeModeWindowManager();
  EXPECT_TRUE(window_state->IsSnapped());
}

// Check that non maximizable windows cannot be dragged by the user.
TEST_F(MaximizeModeWindowManagerTest, TryToDesktopSizeDragUnmaximizable) {
  gfx::Rect rect(10, 10, 100, 100);
  scoped_ptr<aura::Window> window(
      CreateFixedSizeNonMaximizableWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  EXPECT_EQ(rect.ToString(), window->bounds().ToString());

  // 1. Move the mouse over the caption and check that dragging the window does
  // change the location.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.MoveMouseTo(gfx::Point(rect.x() + 2, rect.y() + 2));
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 5);
  RunAllPendingInMessageLoop();
  generator.ReleaseLeftButton();
  gfx::Point first_dragged_origin = window->bounds().origin();
  EXPECT_EQ(rect.x() + 10, first_dragged_origin.x());
  EXPECT_EQ(rect.y() + 5, first_dragged_origin.y());

  // 2. Check that turning on the manager will stop allowing the window from
  // dragging.
  ash::Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(true);
  gfx::Rect center_bounds(window->bounds());
  EXPECT_NE(rect.origin().ToString(), center_bounds.origin().ToString());
  generator.MoveMouseTo(gfx::Point(center_bounds.x() + 1,
                                   center_bounds.y() + 1));
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 5);
  RunAllPendingInMessageLoop();
  generator.ReleaseLeftButton();
  EXPECT_EQ(center_bounds.x(), window->bounds().x());
  EXPECT_EQ(center_bounds.y(), window->bounds().y());
  ash::Shell::GetInstance()->maximize_mode_controller()->
      EnableMaximizeModeWindowManager(false);

  // 3. Releasing the mazimize manager again will restore the window to its
  // previous bounds and
  generator.MoveMouseTo(gfx::Point(first_dragged_origin.x() + 1,
                                   first_dragged_origin.y() + 1));
  generator.PressLeftButton();
  generator.MoveMouseBy(10, 5);
  RunAllPendingInMessageLoop();
  generator.ReleaseLeftButton();
  EXPECT_EQ(first_dragged_origin.x() + 10, window->bounds().x());
  EXPECT_EQ(first_dragged_origin.y() + 5, window->bounds().y());
}

// Test that overview is exited before entering / exiting maximize mode so that
// the window changes made by MaximizeModeWindowManager do not conflict with
// those made in WindowOverview.
TEST_F(MaximizeModeWindowManagerTest, ExitsOverview) {
  // Bounds for windows we know can be controlled.
  gfx::Rect rect1(10, 10, 200, 50);
  gfx::Rect rect2(10, 60, 200, 50);
  scoped_ptr<aura::Window> w1(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect1));
  scoped_ptr<aura::Window> w2(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect2));

  WindowSelectorController* window_selector_controller =
      Shell::GetInstance()->window_selector_controller();
  window_selector_controller->ToggleOverview();
  ASSERT_TRUE(window_selector_controller->IsSelecting());
  ash::MaximizeModeWindowManager* manager = CreateMaximizeModeWindowManager();
  ASSERT_TRUE(manager);
  EXPECT_FALSE(window_selector_controller->IsSelecting());

  window_selector_controller->ToggleOverview();
  ASSERT_TRUE(window_selector_controller->IsSelecting());
  // Destroy the manager again and check that the windows return to their
  // previous state.
  DestroyMaximizeModeWindowManager();
  EXPECT_FALSE(window_selector_controller->IsSelecting());
}

// Test that an edge swipe from the top will end full screen mode.
TEST_F(MaximizeModeWindowManagerTest, ExitFullScreenWithEdgeSwipeFromTop) {
  gfx::Rect rect(10, 10, 200, 50);
  scoped_ptr<aura::Window>
      background_window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window>
      foreground_window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* background_window_state =
      wm::GetWindowState(background_window.get());
  wm::WindowState* foreground_window_state =
      wm::GetWindowState(foreground_window.get());
  wm::ActivateWindow(foreground_window.get());
  CreateMaximizeModeWindowManager();

  // Fullscreen both windows.
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  background_window_state->OnWMEvent(&event);
  foreground_window_state->OnWMEvent(&event);
  EXPECT_TRUE(background_window_state->IsFullscreen());
  EXPECT_TRUE(foreground_window_state->IsFullscreen());
  EXPECT_EQ(foreground_window.get(), wm::GetActiveWindow());

  // Do an edge swipe top into screen.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.GestureScrollSequence(gfx::Point(50, 0),
                                  gfx::Point(50, 100),
                                  base::TimeDelta::FromMilliseconds(20),
                                  10);

  EXPECT_FALSE(foreground_window_state->IsFullscreen());
  EXPECT_TRUE(background_window_state->IsFullscreen());

  // Do a second edge swipe top into screen.
  generator.GestureScrollSequence(gfx::Point(50, 0),
                                  gfx::Point(50, 100),
                                  base::TimeDelta::FromMilliseconds(20),
                                  10);

  EXPECT_FALSE(foreground_window_state->IsFullscreen());
  EXPECT_TRUE(background_window_state->IsFullscreen());

  DestroyMaximizeModeWindowManager();
}

// Test that an edge swipe from the bottom will end full screen mode.
TEST_F(MaximizeModeWindowManagerTest, ExitFullScreenWithEdgeSwipeFromBottom) {
  gfx::Rect rect(10, 10, 200, 50);
  scoped_ptr<aura::Window>
      background_window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window>
      foreground_window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* background_window_state =
      wm::GetWindowState(background_window.get());
  wm::WindowState* foreground_window_state =
      wm::GetWindowState(foreground_window.get());
  wm::ActivateWindow(foreground_window.get());
  CreateMaximizeModeWindowManager();

  // Fullscreen both windows.
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  background_window_state->OnWMEvent(&event);
  foreground_window_state->OnWMEvent(&event);
  EXPECT_TRUE(background_window_state->IsFullscreen());
  EXPECT_TRUE(foreground_window_state->IsFullscreen());
  EXPECT_EQ(foreground_window.get(), wm::GetActiveWindow());

  // Do an edge swipe bottom into screen.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  int y = Shell::GetPrimaryRootWindow()->bounds().bottom();
  generator.GestureScrollSequence(gfx::Point(50, y),
                                  gfx::Point(50, y - 100),
                                  base::TimeDelta::FromMilliseconds(20),
                                  10);

  EXPECT_FALSE(foreground_window_state->IsFullscreen());
  EXPECT_TRUE(background_window_state->IsFullscreen());

  DestroyMaximizeModeWindowManager();
}

// Test that an edge touch press at the top will end full screen mode.
TEST_F(MaximizeModeWindowManagerTest, ExitFullScreenWithEdgeTouchAtTop) {
  gfx::Rect rect(10, 10, 200, 50);
  scoped_ptr<aura::Window>
      background_window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window>
      foreground_window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* background_window_state =
      wm::GetWindowState(background_window.get());
  wm::WindowState* foreground_window_state =
      wm::GetWindowState(foreground_window.get());
  wm::ActivateWindow(foreground_window.get());
  CreateMaximizeModeWindowManager();

  // Fullscreen both windows.
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  background_window_state->OnWMEvent(&event);
  foreground_window_state->OnWMEvent(&event);
  EXPECT_TRUE(background_window_state->IsFullscreen());
  EXPECT_TRUE(foreground_window_state->IsFullscreen());
  EXPECT_EQ(foreground_window.get(), wm::GetActiveWindow());

  // Touch tap on the top edge.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.GestureTapAt(gfx::Point(100, 0));
  EXPECT_FALSE(foreground_window_state->IsFullscreen());
  EXPECT_TRUE(background_window_state->IsFullscreen());

  // Try the same again and see that nothing changes.
  generator.GestureTapAt(gfx::Point(100, 0));
  EXPECT_FALSE(foreground_window_state->IsFullscreen());
  EXPECT_TRUE(background_window_state->IsFullscreen());

  DestroyMaximizeModeWindowManager();
}

// Test that an edge touch press at the bottom will end full screen mode.
TEST_F(MaximizeModeWindowManagerTest, ExitFullScreenWithEdgeTouchAtBottom) {
  gfx::Rect rect(10, 10, 200, 50);
  scoped_ptr<aura::Window>
      background_window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  scoped_ptr<aura::Window>
      foreground_window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL, rect));
  wm::WindowState* background_window_state =
      wm::GetWindowState(background_window.get());
  wm::WindowState* foreground_window_state =
      wm::GetWindowState(foreground_window.get());
  wm::ActivateWindow(foreground_window.get());
  CreateMaximizeModeWindowManager();

  // Fullscreen both windows.
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  background_window_state->OnWMEvent(&event);
  foreground_window_state->OnWMEvent(&event);
  EXPECT_TRUE(background_window_state->IsFullscreen());
  EXPECT_TRUE(foreground_window_state->IsFullscreen());
  EXPECT_EQ(foreground_window.get(), wm::GetActiveWindow());

  // Touch tap on the bottom edge.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.GestureTapAt(
      gfx::Point(100, Shell::GetPrimaryRootWindow()->bounds().bottom() - 1));
  EXPECT_FALSE(foreground_window_state->IsFullscreen());
  EXPECT_TRUE(background_window_state->IsFullscreen());

  // Try the same again and see that nothing changes.
  generator.GestureTapAt(
      gfx::Point(100, Shell::GetPrimaryRootWindow()->bounds().bottom() - 1));
  EXPECT_FALSE(foreground_window_state->IsFullscreen());
  EXPECT_TRUE(background_window_state->IsFullscreen());

  DestroyMaximizeModeWindowManager();
}

// Test that an edge swipe from the top on an immersive mode window will not end
// full screen mode.
TEST_F(MaximizeModeWindowManagerTest, NoExitImmersiveModeWithEdgeSwipeFromTop) {
  scoped_ptr<aura::Window> window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL,
                                  gfx::Rect(10, 10, 200, 50)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  wm::ActivateWindow(window.get());
  CreateMaximizeModeWindowManager();

  // Fullscreen the window.
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event);
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->in_immersive_fullscreen());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());

  window_state->set_in_immersive_fullscreen(true);

  // Do an edge swipe top into screen.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  generator.GestureScrollSequence(gfx::Point(50, 0),
                                  gfx::Point(50, 100),
                                  base::TimeDelta::FromMilliseconds(20),
                                  10);

  // It should have not exited full screen or immersive mode.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->in_immersive_fullscreen());

  DestroyMaximizeModeWindowManager();
}

// Test that an edge swipe from the bottom will not end immersive mode.
TEST_F(MaximizeModeWindowManagerTest,
       NoExitImmersiveModeWithEdgeSwipeFromBottom) {
  scoped_ptr<aura::Window> window(CreateWindow(ui::wm::WINDOW_TYPE_NORMAL,
                                               gfx::Rect(10, 10, 200, 50)));
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  wm::ActivateWindow(window.get());
  CreateMaximizeModeWindowManager();

  // Fullscreen the window.
  wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event);
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->in_immersive_fullscreen());
  EXPECT_EQ(window.get(), wm::GetActiveWindow());
  window_state->set_in_immersive_fullscreen(true);
  EXPECT_TRUE(window_state->in_immersive_fullscreen());

  // Do an edge swipe bottom into screen.
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  int y = Shell::GetPrimaryRootWindow()->bounds().bottom();
  generator.GestureScrollSequence(gfx::Point(50, y),
                                  gfx::Point(50, y - 100),
                                  base::TimeDelta::FromMilliseconds(20),
                                  10);

  // The window should still be full screen and immersive.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->in_immersive_fullscreen());

  DestroyMaximizeModeWindowManager();
}

#endif  // OS_WIN

}  // namespace ash
