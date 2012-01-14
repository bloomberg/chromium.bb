// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"
#include "ash/ime/event.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/aura_shell_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif

namespace ash {
namespace test {

namespace {

class TestTarget : public ui::AcceleratorTarget {
 public:
  TestTarget() : accelerator_pressed_count_(0) {};
  virtual ~TestTarget() {};

  int accelerator_pressed_count() const {
    return accelerator_pressed_count_;
  }

  void set_accelerator_pressed_count(int accelerator_pressed_count) {
    accelerator_pressed_count_ = accelerator_pressed_count;
  }

  // Overridden from ui::AcceleratorTarget:
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;
  virtual bool CanHandleAccelerators() const OVERRIDE;

 private:
  int accelerator_pressed_count_;

  DISALLOW_COPY_AND_ASSIGN(TestTarget);
};

class DummyScreenshotDelegate : public ScreenshotDelegate {
 public:
  DummyScreenshotDelegate() : handle_take_screenshot_count_(0) {
  }
  virtual ~DummyScreenshotDelegate() {}

  // Overridden from ScreenshotDelegate:
  virtual void HandleTakeScreenshot() OVERRIDE {
    ++handle_take_screenshot_count_;
  }

  int handle_take_screenshot_count() const {
    return handle_take_screenshot_count_;
  }

 private:
  int handle_take_screenshot_count_;

  DISALLOW_COPY_AND_ASSIGN(DummyScreenshotDelegate);
};

bool TestTarget::AcceleratorPressed(const ui::Accelerator& accelerator) {
  ++accelerator_pressed_count_;
  return true;
}

bool TestTarget::CanHandleAccelerators() const {
  return true;
}

}  // namespace

class AcceleratorControllerTest : public AuraShellTestBase {
 public:
  AcceleratorControllerTest() {};
  virtual ~AcceleratorControllerTest() {};

  static AcceleratorController* GetController();
};

AcceleratorController* AcceleratorControllerTest::GetController() {
  return Shell::GetInstance()->accelerator_controller();
}

TEST_F(AcceleratorControllerTest, Register) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);

  // The registered accelerator is processed.
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, RegisterMultipleTarget) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);
  TestTarget target2;
  GetController()->Register(accelerator_a, &target2);

  // If multiple targets are registered with the same accelerator, the target
  // registered later processes the accelerator.
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_EQ(0, target1.accelerator_pressed_count());
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, Unregister) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);
  const ui::Accelerator accelerator_b(ui::VKEY_B, false, false, false);
  GetController()->Register(accelerator_b, &target);

  // Unregistering a different accelerator does not affect the other
  // accelerator.
  GetController()->Unregister(accelerator_b, &target);
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());

  // The unregistered accelerator is no longer processed.
  target.set_accelerator_pressed_count(0);
  GetController()->Unregister(accelerator_a, &target);
  EXPECT_FALSE(GetController()->Process(accelerator_a));
  EXPECT_EQ(0, target.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, UnregisterAll) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);
  const ui::Accelerator accelerator_b(ui::VKEY_B, false, false, false);
  GetController()->Register(accelerator_b, &target1);
  const ui::Accelerator accelerator_c(ui::VKEY_C, false, false, false);
  TestTarget target2;
  GetController()->Register(accelerator_c, &target2);
  GetController()->UnregisterAll(&target1);

  // All the accelerators registered for |target1| are no longer processed.
  EXPECT_FALSE(GetController()->Process(accelerator_a));
  EXPECT_FALSE(GetController()->Process(accelerator_b));
  EXPECT_EQ(0, target1.accelerator_pressed_count());

  // UnregisterAll with a different target does not affect the other target.
  EXPECT_TRUE(GetController()->Process(accelerator_c));
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, Process) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);

  // The registered accelerator is processed.
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_EQ(1, target1.accelerator_pressed_count());

  // The non-registered accelerator is not processed.
  const ui::Accelerator accelerator_b(ui::VKEY_B, false, false, false);
  EXPECT_FALSE(GetController()->Process(accelerator_b));
}

#if defined(OS_WIN) || defined(USE_X11)
TEST_F(AcceleratorControllerTest, ProcessOnce) {
  // A focused window must exist for accelerators to be processed.
  aura::Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  aura::Window* window = aura::test::CreateTestWindowWithDelegate(
      new aura::test::TestWindowDelegate,
      -1,
      gfx::Rect(),
      default_container);
  ActivateWindow(window);

  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);

  // The accelerator is processed only once.
#if defined(OS_WIN)
  MSG msg1 = { NULL, WM_KEYDOWN, ui::VKEY_A, 0 };
  TranslatedKeyEvent key_event1(msg1, false);
  EXPECT_TRUE(aura::RootWindow::GetInstance()->DispatchKeyEvent(&key_event1));

  MSG msg2 = { NULL, WM_CHAR, L'A', 0 };
  TranslatedKeyEvent key_event2(msg2, true);
  EXPECT_FALSE(aura::RootWindow::GetInstance()->DispatchKeyEvent(&key_event2));

  MSG msg3 = { NULL, WM_KEYUP, ui::VKEY_A, 0 };
  TranslatedKeyEvent key_event3(msg3, false);
  EXPECT_FALSE(aura::RootWindow::GetInstance()->DispatchKeyEvent(&key_event3));
#elif defined(USE_X11)
  XEvent key_event;
  ui::InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                              ui::VKEY_A,
                              0,
                              &key_event);
  TranslatedKeyEvent key_event1(&key_event, false);
  EXPECT_TRUE(aura::RootWindow::GetInstance()->DispatchKeyEvent(&key_event1));

  TranslatedKeyEvent key_event2(&key_event, true);
  EXPECT_FALSE(aura::RootWindow::GetInstance()->DispatchKeyEvent(&key_event2));

  ui::InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                              ui::VKEY_A,
                              0,
                              &key_event);
  TranslatedKeyEvent key_event3(&key_event, false);
  EXPECT_FALSE(aura::RootWindow::GetInstance()->DispatchKeyEvent(&key_event3));
#endif
  EXPECT_EQ(1, target.accelerator_pressed_count());
}
#endif

TEST_F(AcceleratorControllerTest, GlobalAccelerators) {
  // A focused window must exist for accelerators to be processed.
  aura::Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  aura::Window* window = aura::test::CreateTestWindowWithDelegate(
      new aura::test::TestWindowDelegate,
      -1,
      gfx::Rect(),
      default_container);
  ActivateWindow(window);

  // CycleBackward
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F5, true, false, false)));
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_TAB, true, false, true)));
  // CycleForward
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F5, false, false, false)));
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_TAB, false, false, true)));
  // TakeScreenshot
  // True should always be returned regardless of the existence of the delegate.
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F5, false, true, false)));
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_PRINT, false, false, false)));
  DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
  GetController()->SetScreenshotDelegate(delegate);
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F5, false, true, false)));
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_PRINT, false, false, false)));
  EXPECT_EQ(2, delegate->handle_take_screenshot_count());
#if !defined(NDEBUG)
  // RotateScreen
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_HOME, false, true, false)));
#if !defined(OS_LINUX)
  // ToggleDesktopFullScreen (not implemented yet on Linux)
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F11, false, true, false)));
#endif
#endif
}

}  // namespace test
}  // namespace ash
