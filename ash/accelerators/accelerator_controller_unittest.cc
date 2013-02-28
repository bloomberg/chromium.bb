// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/caps_lock_delegate.h"
#include "ash/display/display_manager.h"
#include "ash/ime_control_delegate.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/system/keyboard_brightness/keyboard_brightness_control_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/window_util.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"

#if defined(USE_X11)
#include <X11/Xlib.h>
#include "ui/base/x/x11_util.h"
#endif

namespace ash {

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

class ReleaseAccelerator : public ui::Accelerator {
 public:
  ReleaseAccelerator(ui::KeyboardCode keycode, int modifiers)
      : ui::Accelerator(keycode, modifiers) {
    set_type(ui::ET_KEY_RELEASED);
  }
};

class DummyScreenshotDelegate : public ScreenshotDelegate {
 public:
  DummyScreenshotDelegate()
      : handle_take_screenshot_count_(0) {
  }
  virtual ~DummyScreenshotDelegate() {}

  // Overridden from ScreenshotDelegate:
  virtual void HandleTakeScreenshotForAllRootWindows() OVERRIDE {
    ++handle_take_screenshot_count_;
  }

  virtual void HandleTakePartialScreenshot(
      aura::Window* window, const gfx::Rect& rect) OVERRIDE {
  }

  virtual bool CanTakeScreenshot() OVERRIDE {
    return true;
  }

  int handle_take_screenshot_count() const {
    return handle_take_screenshot_count_;
  }

 private:
  int handle_take_screenshot_count_;

  DISALLOW_COPY_AND_ASSIGN(DummyScreenshotDelegate);
};

class DummyVolumeControlDelegate : public VolumeControlDelegate {
 public:
  explicit DummyVolumeControlDelegate(bool consume)
      : consume_(consume),
        handle_volume_mute_count_(0),
        handle_volume_down_count_(0),
        handle_volume_up_count_(0) {
  }
  virtual ~DummyVolumeControlDelegate() {}

  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) OVERRIDE {
    ++handle_volume_mute_count_;
    last_accelerator_ = accelerator;
    return consume_;
  }
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) OVERRIDE {
    ++handle_volume_down_count_;
    last_accelerator_ = accelerator;
    return consume_;
  }
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) OVERRIDE {
    ++handle_volume_up_count_;
    last_accelerator_ = accelerator;
    return consume_;
  }
  virtual void SetVolumePercent(double percent) OVERRIDE {
  }
  virtual bool IsAudioMuted() const OVERRIDE {
    return false;
  }
  virtual void SetAudioMuted(bool muted) OVERRIDE {
  }
  virtual float GetVolumeLevel() const OVERRIDE {
    return 0.0;
  }
  virtual void SetVolumeLevel(float level) OVERRIDE {
  }

  int handle_volume_mute_count() const {
    return handle_volume_mute_count_;
  }
  int handle_volume_down_count() const {
    return handle_volume_down_count_;
  }
  int handle_volume_up_count() const {
    return handle_volume_up_count_;
  }
  const ui::Accelerator& last_accelerator() const {
    return last_accelerator_;
  }

 private:
  const bool consume_;
  int handle_volume_mute_count_;
  int handle_volume_down_count_;
  int handle_volume_up_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(DummyVolumeControlDelegate);
};

class DummyBrightnessControlDelegate : public BrightnessControlDelegate {
 public:
  explicit DummyBrightnessControlDelegate(bool consume)
      : consume_(consume),
        handle_brightness_down_count_(0),
        handle_brightness_up_count_(0) {
  }
  virtual ~DummyBrightnessControlDelegate() {}

  virtual bool HandleBrightnessDown(
      const ui::Accelerator& accelerator) OVERRIDE {
    ++handle_brightness_down_count_;
    last_accelerator_ = accelerator;
    return consume_;
  }
  virtual bool HandleBrightnessUp(const ui::Accelerator& accelerator) OVERRIDE {
    ++handle_brightness_up_count_;
    last_accelerator_ = accelerator;
    return consume_;
  }
  virtual void SetBrightnessPercent(double percent, bool gradual) OVERRIDE {}
  virtual void GetBrightnessPercent(
      const base::Callback<void(double)>& callback) OVERRIDE {
    callback.Run(100.0);
  }

  int handle_brightness_down_count() const {
    return handle_brightness_down_count_;
  }
  int handle_brightness_up_count() const {
    return handle_brightness_up_count_;
  }
  const ui::Accelerator& last_accelerator() const {
    return last_accelerator_;
  }

 private:
  const bool consume_;
  int handle_brightness_down_count_;
  int handle_brightness_up_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(DummyBrightnessControlDelegate);
};

class DummyImeControlDelegate : public ImeControlDelegate {
 public:
  explicit DummyImeControlDelegate(bool consume)
      : consume_(consume),
        handle_next_ime_count_(0),
        handle_previous_ime_count_(0),
        handle_switch_ime_count_(0) {
  }
  virtual ~DummyImeControlDelegate() {}

  virtual bool HandleNextIme() OVERRIDE {
    ++handle_next_ime_count_;
    return consume_;
  }
  virtual bool HandlePreviousIme() OVERRIDE {
    ++handle_previous_ime_count_;
    return consume_;
  }
  virtual bool HandleSwitchIme(const ui::Accelerator& accelerator) OVERRIDE {
    ++handle_switch_ime_count_;
    last_accelerator_ = accelerator;
    return consume_;
  }

  int handle_next_ime_count() const {
    return handle_next_ime_count_;
  }
  int handle_previous_ime_count() const {
    return handle_previous_ime_count_;
  }
  int handle_switch_ime_count() const {
    return handle_switch_ime_count_;
  }
  const ui::Accelerator& last_accelerator() const {
    return last_accelerator_;
  }
  virtual ui::Accelerator RemapAccelerator(
      const ui::Accelerator& accelerator) OVERRIDE {
    return ui::Accelerator(accelerator);
  }

 private:
  const bool consume_;
  int handle_next_ime_count_;
  int handle_previous_ime_count_;
  int handle_switch_ime_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(DummyImeControlDelegate);
};

class DummyKeyboardBrightnessControlDelegate
    : public KeyboardBrightnessControlDelegate {
 public:
  explicit DummyKeyboardBrightnessControlDelegate(bool consume)
      : consume_(consume),
        handle_keyboard_brightness_down_count_(0),
        handle_keyboard_brightness_up_count_(0) {
  }
  virtual ~DummyKeyboardBrightnessControlDelegate() {}

  virtual bool HandleKeyboardBrightnessDown(
      const ui::Accelerator& accelerator) OVERRIDE {
    ++handle_keyboard_brightness_down_count_;
    last_accelerator_ = accelerator;
    return consume_;
  }

  virtual bool HandleKeyboardBrightnessUp(
      const ui::Accelerator& accelerator) OVERRIDE {
    ++handle_keyboard_brightness_up_count_;
    last_accelerator_ = accelerator;
    return consume_;
  }

  int handle_keyboard_brightness_down_count() const {
    return handle_keyboard_brightness_down_count_;
  }

  int handle_keyboard_brightness_up_count() const {
    return handle_keyboard_brightness_up_count_;
  }

  const ui::Accelerator& last_accelerator() const {
    return last_accelerator_;
  }

 private:
  const bool consume_;
  int handle_keyboard_brightness_down_count_;
  int handle_keyboard_brightness_up_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(DummyKeyboardBrightnessControlDelegate);
};

bool TestTarget::AcceleratorPressed(const ui::Accelerator& accelerator) {
  ++accelerator_pressed_count_;
  return true;
}

bool TestTarget::CanHandleAccelerators() const {
  return true;
}

}  // namespace

class AcceleratorControllerTest : public test::AshTestBase {
 public:
  AcceleratorControllerTest() {};
  virtual ~AcceleratorControllerTest() {};

 protected:
  void EnableInternalDisplay() {
    Shell::GetInstance()->display_manager()->
        SetFirstDisplayAsInternalDisplayForTest();
  }

  static AcceleratorController* GetController();
  static bool ProcessWithContext(const ui::Accelerator& accelerator);

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerTest);
};

AcceleratorController* AcceleratorControllerTest::GetController() {
  return Shell::GetInstance()->accelerator_controller();
}

bool AcceleratorControllerTest::ProcessWithContext(
    const ui::Accelerator& accelerator) {
  AcceleratorController* controller = GetController();
  controller->context()->UpdateContext(accelerator);
  return controller->Process(accelerator);
}

TEST_F(AcceleratorControllerTest, Register) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);

  // The registered accelerator is processed.
  EXPECT_TRUE(ProcessWithContext(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, RegisterMultipleTarget) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);
  TestTarget target2;
  GetController()->Register(accelerator_a, &target2);

  // If multiple targets are registered with the same accelerator, the target
  // registered later processes the accelerator.
  EXPECT_TRUE(ProcessWithContext(accelerator_a));
  EXPECT_EQ(0, target1.accelerator_pressed_count());
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, Unregister) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  GetController()->Register(accelerator_b, &target);

  // Unregistering a different accelerator does not affect the other
  // accelerator.
  GetController()->Unregister(accelerator_b, &target);
  EXPECT_TRUE(ProcessWithContext(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());

  // The unregistered accelerator is no longer processed.
  target.set_accelerator_pressed_count(0);
  GetController()->Unregister(accelerator_a, &target);
  EXPECT_FALSE(ProcessWithContext(accelerator_a));
  EXPECT_EQ(0, target.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, UnregisterAll) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  GetController()->Register(accelerator_b, &target1);
  const ui::Accelerator accelerator_c(ui::VKEY_C, ui::EF_NONE);
  TestTarget target2;
  GetController()->Register(accelerator_c, &target2);
  GetController()->UnregisterAll(&target1);

  // All the accelerators registered for |target1| are no longer processed.
  EXPECT_FALSE(ProcessWithContext(accelerator_a));
  EXPECT_FALSE(ProcessWithContext(accelerator_b));
  EXPECT_EQ(0, target1.accelerator_pressed_count());

  // UnregisterAll with a different target does not affect the other target.
  EXPECT_TRUE(ProcessWithContext(accelerator_c));
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, Process) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);

  // The registered accelerator is processed.
  EXPECT_TRUE(ProcessWithContext(accelerator_a));
  EXPECT_EQ(1, target1.accelerator_pressed_count());

  // The non-registered accelerator is not processed.
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  EXPECT_FALSE(ProcessWithContext(accelerator_b));
}

TEST_F(AcceleratorControllerTest, IsRegistered) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  const ui::Accelerator accelerator_shift_a(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);
  EXPECT_TRUE(GetController()->IsRegistered(accelerator_a));
  EXPECT_FALSE(GetController()->IsRegistered(accelerator_shift_a));
  GetController()->UnregisterAll(&target);
  EXPECT_FALSE(GetController()->IsRegistered(accelerator_a));
}

TEST_F(AcceleratorControllerTest, WindowSnap) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  const ui::Accelerator dummy;

  wm::ActivateWindow(window.get());

  {
    GetController()->PerformAction(WINDOW_SNAP_LEFT, dummy);
    gfx::Rect snap_left = window->bounds();
    GetController()->PerformAction(WINDOW_SNAP_LEFT, dummy);
    EXPECT_NE(window->bounds().ToString(), snap_left.ToString());
    GetController()->PerformAction(WINDOW_SNAP_LEFT, dummy);
    EXPECT_NE(window->bounds().ToString(), snap_left.ToString());

    // It should cycle back to the first snapped position.
    GetController()->PerformAction(WINDOW_SNAP_LEFT, dummy);
    EXPECT_EQ(window->bounds().ToString(), snap_left.ToString());
  }
  {
    GetController()->PerformAction(WINDOW_SNAP_RIGHT, dummy);
    gfx::Rect snap_right = window->bounds();
    GetController()->PerformAction(WINDOW_SNAP_RIGHT, dummy);
    EXPECT_NE(window->bounds().ToString(), snap_right.ToString());
    GetController()->PerformAction(WINDOW_SNAP_RIGHT, dummy);
    EXPECT_NE(window->bounds().ToString(), snap_right.ToString());

    // It should cycle back to the first snapped position.
    GetController()->PerformAction(WINDOW_SNAP_RIGHT, dummy);
    EXPECT_EQ(window->bounds().ToString(), snap_right.ToString());
  }
  {
    gfx::Rect normal_bounds = window->bounds();

    GetController()->PerformAction(TOGGLE_MAXIMIZED, dummy);
    EXPECT_TRUE(wm::IsWindowMaximized(window.get()));
    EXPECT_NE(normal_bounds.ToString(), window->bounds().ToString());

    GetController()->PerformAction(TOGGLE_MAXIMIZED, dummy);
    EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
    EXPECT_EQ(normal_bounds.ToString(), window->bounds().ToString());

    GetController()->PerformAction(TOGGLE_MAXIMIZED, dummy);
    GetController()->PerformAction(WINDOW_SNAP_LEFT, dummy);
    EXPECT_FALSE(wm::IsWindowMaximized(window.get()));

    GetController()->PerformAction(TOGGLE_MAXIMIZED, dummy);
    GetController()->PerformAction(WINDOW_SNAP_RIGHT, dummy);
    EXPECT_FALSE(wm::IsWindowMaximized(window.get()));

    GetController()->PerformAction(TOGGLE_MAXIMIZED, dummy);
    EXPECT_TRUE(wm::IsWindowMaximized(window.get()));
    GetController()->PerformAction(WINDOW_MINIMIZE, dummy);
    EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
    EXPECT_TRUE(wm::IsWindowMinimized(window.get()));
    wm::RestoreWindow(window.get());
    wm::ActivateWindow(window.get());
  }
  {
    GetController()->PerformAction(WINDOW_MINIMIZE, dummy);
    EXPECT_TRUE(wm::IsWindowMinimized(window.get()));
  }
}

TEST_F(AcceleratorControllerTest, ControllerContext) {
  ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  ui::Accelerator accelerator_a2(ui::VKEY_A, ui::EF_NONE);
  ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);

  accelerator_a.set_type(ui::ET_KEY_PRESSED);
  accelerator_a2.set_type(ui::ET_KEY_RELEASED);
  accelerator_b.set_type(ui::ET_KEY_PRESSED);

  EXPECT_FALSE(GetController()->context()->repeated());
  EXPECT_EQ(ui::ET_UNKNOWN,
            GetController()->context()->previous_accelerator().type());

  GetController()->context()->UpdateContext(accelerator_a);
  EXPECT_FALSE(GetController()->context()->repeated());
  EXPECT_EQ(ui::ET_UNKNOWN,
            GetController()->context()->previous_accelerator().type());

  GetController()->context()->UpdateContext(accelerator_a2);
  EXPECT_FALSE(GetController()->context()->repeated());
  EXPECT_EQ(ui::ET_KEY_PRESSED,
            GetController()->context()->previous_accelerator().type());

  GetController()->context()->UpdateContext(accelerator_a2);
  EXPECT_TRUE(GetController()->context()->repeated());
  EXPECT_EQ(ui::ET_KEY_RELEASED,
            GetController()->context()->previous_accelerator().type());

  GetController()->context()->UpdateContext(accelerator_b);
  EXPECT_FALSE(GetController()->context()->repeated());
  EXPECT_EQ(ui::ET_KEY_RELEASED,
            GetController()->context()->previous_accelerator().type());
}

TEST_F(AcceleratorControllerTest, SuppressToggleMaximized) {
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  wm::ActivateWindow(window.get());
  const ui::Accelerator accelerator(ui::VKEY_A, ui::EF_NONE);
  const ui::Accelerator empty_accelerator;

  // Toggling not suppressed.
  GetController()->context()->UpdateContext(accelerator);
  GetController()->PerformAction(TOGGLE_MAXIMIZED, accelerator);
  EXPECT_TRUE(wm::IsWindowMaximized(window.get()));

  // The same accelerator - toggling suppressed.
  GetController()->context()->UpdateContext(accelerator);
  GetController()->PerformAction(TOGGLE_MAXIMIZED, accelerator);
  EXPECT_TRUE(wm::IsWindowMaximized(window.get()));

  // Suppressed but not for gesture events.
  GetController()->PerformAction(TOGGLE_MAXIMIZED, empty_accelerator);
  EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
}

#if defined(OS_WIN) || defined(USE_X11)
TEST_F(AcceleratorControllerTest, ProcessOnce) {
  ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);

  // The accelerator is processed only once.
#if defined(OS_WIN)
  MSG msg1 = { NULL, WM_KEYDOWN, ui::VKEY_A, 0 };
  ui::TranslatedKeyEvent key_event1(msg1, false);
  EXPECT_TRUE(Shell::GetPrimaryRootWindow()->AsRootWindowHostDelegate()->
      OnHostKeyEvent(&key_event1));

  MSG msg2 = { NULL, WM_CHAR, L'A', 0 };
  ui::TranslatedKeyEvent key_event2(msg2, true);
  EXPECT_FALSE(Shell::GetPrimaryRootWindow()->AsRootWindowHostDelegate()->
      OnHostKeyEvent(&key_event2));

  MSG msg3 = { NULL, WM_KEYUP, ui::VKEY_A, 0 };
  ui::TranslatedKeyEvent key_event3(msg3, false);
  EXPECT_FALSE(Shell::GetPrimaryRootWindow()->AsRootWindowHostDelegate()->
      OnHostKeyEvent(&key_event3));
#elif defined(USE_X11)
  XEvent key_event;
  ui::InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                              ui::VKEY_A,
                              0,
                              &key_event);
  ui::TranslatedKeyEvent key_event1(&key_event, false);
  EXPECT_TRUE(Shell::GetPrimaryRootWindow()->AsRootWindowHostDelegate()->
      OnHostKeyEvent(&key_event1));

  ui::TranslatedKeyEvent key_event2(&key_event, true);
  EXPECT_FALSE(Shell::GetPrimaryRootWindow()->AsRootWindowHostDelegate()->
      OnHostKeyEvent(&key_event2));

  ui::InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                              ui::VKEY_A,
                              0,
                              &key_event);
  ui::TranslatedKeyEvent key_event3(&key_event, false);
  EXPECT_FALSE(Shell::GetPrimaryRootWindow()->AsRootWindowHostDelegate()->
      OnHostKeyEvent(&key_event3));
#endif
  EXPECT_EQ(1, target.accelerator_pressed_count());
}
#endif

TEST_F(AcceleratorControllerTest, GlobalAccelerators) {
  // CycleBackward
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
  // CycleForward
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
#if defined(OS_CHROMEOS)
  // CycleBackward
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_SHIFT_DOWN)));
  // CycleForward
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_NONE)));

  // Take screenshot / partial screenshot
  // True should always be returned regardless of the existence of the delegate.
  {
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1,
                        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
    DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
    GetController()->SetScreenshotDelegate(
        scoped_ptr<ScreenshotDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(1, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1,
                        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
  }
#endif
  // DisableCapsLock
  {
    CapsLockDelegate* delegate = Shell::GetInstance()->caps_lock_delegate();
    delegate->SetCapsLockEnabled(true);
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    // Handled only on key release.
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_LSHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_TRUE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_SHIFT, ui::EF_NONE)));
    EXPECT_FALSE(delegate->IsCapsLockEnabled());
    delegate->SetCapsLockEnabled(true);
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_RSHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_TRUE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_LSHIFT, ui::EF_NONE)));
    EXPECT_FALSE(delegate->IsCapsLockEnabled());
    delegate->SetCapsLockEnabled(true);
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_SHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_TRUE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_RSHIFT, ui::EF_NONE)));
    EXPECT_FALSE(delegate->IsCapsLockEnabled());

    // Do not handle when a shift pressed with other keys.
    delegate->SetCapsLockEnabled(true);
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_A, ui::EF_SHIFT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_FALSE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_A, ui::EF_SHIFT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());

    // Do not handle when a shift pressed with other keys, and shift is
    // released first.
    delegate->SetCapsLockEnabled(true);
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_A, ui::EF_SHIFT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_FALSE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_LSHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());

    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_A, ui::EF_SHIFT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_FALSE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_SHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());

    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_A, ui::EF_SHIFT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_FALSE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_RSHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());

    // Do not consume shift keyup when caps lock is off.
    delegate->SetCapsLockEnabled(false);
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_LSHIFT, ui::EF_NONE)));
    EXPECT_FALSE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_LSHIFT, ui::EF_NONE)));
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_RSHIFT, ui::EF_NONE)));
    EXPECT_FALSE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_RSHIFT, ui::EF_NONE)));
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_SHIFT, ui::EF_NONE)));
    EXPECT_FALSE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_SHIFT, ui::EF_NONE)));
  }
  // ToggleCapsLock
  {
    CapsLockDelegate* delegate = Shell::GetInstance()->caps_lock_delegate();
    delegate->SetCapsLockEnabled(true);
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_ALT_DOWN)));
    EXPECT_TRUE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_ALT_DOWN)));
    EXPECT_FALSE(delegate->IsCapsLockEnabled());
    EXPECT_FALSE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_ALT_DOWN)));
    EXPECT_TRUE(ProcessWithContext(
        ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_ALT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
  }
  const ui::Accelerator volume_mute(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  const ui::Accelerator volume_down(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  const ui::Accelerator volume_up(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  {
    DummyVolumeControlDelegate* delegate =
        new DummyVolumeControlDelegate(false);
    ash::Shell::GetInstance()->system_tray_delegate()->SetVolumeControlDelegate(
        scoped_ptr<VolumeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_volume_mute_count());
    EXPECT_FALSE(ProcessWithContext(volume_mute));
    EXPECT_EQ(1, delegate->handle_volume_mute_count());
    EXPECT_EQ(volume_mute, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_down_count());
    EXPECT_FALSE(ProcessWithContext(volume_down));
    EXPECT_EQ(1, delegate->handle_volume_down_count());
    EXPECT_EQ(volume_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_up_count());
    EXPECT_FALSE(ProcessWithContext(volume_up));
    EXPECT_EQ(1, delegate->handle_volume_up_count());
    EXPECT_EQ(volume_up, delegate->last_accelerator());
  }
  {
    DummyVolumeControlDelegate* delegate = new DummyVolumeControlDelegate(true);
    ash::Shell::GetInstance()->system_tray_delegate()->SetVolumeControlDelegate(
        scoped_ptr<VolumeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_volume_mute_count());
    EXPECT_TRUE(ProcessWithContext(volume_mute));
    EXPECT_EQ(1, delegate->handle_volume_mute_count());
    EXPECT_EQ(volume_mute, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_down_count());
    EXPECT_TRUE(ProcessWithContext(volume_down));
    EXPECT_EQ(1, delegate->handle_volume_down_count());
    EXPECT_EQ(volume_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_up_count());
    EXPECT_TRUE(ProcessWithContext(volume_up));
    EXPECT_EQ(1, delegate->handle_volume_up_count());
    EXPECT_EQ(volume_up, delegate->last_accelerator());
  }
#if defined(OS_CHROMEOS)
  // Brightness
  // ui::VKEY_BRIGHTNESS_DOWN/UP are not defined on Windows.
  const ui::Accelerator brightness_down(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  const ui::Accelerator brightness_up(ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  {
    EXPECT_FALSE(ProcessWithContext(brightness_down));
    EXPECT_FALSE(ProcessWithContext(brightness_up));
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(true);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_FALSE(ProcessWithContext(brightness_down));
    EXPECT_FALSE(ProcessWithContext(brightness_up));
  }
  // Enable internal display.
  EnableInternalDisplay();
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(false);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_FALSE(ProcessWithContext(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_FALSE(ProcessWithContext(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(true);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_TRUE(ProcessWithContext(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_TRUE(ProcessWithContext(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }

  // Keyboard brightness
  const ui::Accelerator alt_brightness_down(ui::VKEY_BRIGHTNESS_DOWN,
                                            ui::EF_ALT_DOWN);
  const ui::Accelerator alt_brightness_up(ui::VKEY_BRIGHTNESS_UP,
                                          ui::EF_ALT_DOWN);
  {
    EXPECT_TRUE(ProcessWithContext(alt_brightness_down));
    EXPECT_TRUE(ProcessWithContext(alt_brightness_up));
    DummyKeyboardBrightnessControlDelegate* delegate =
        new DummyKeyboardBrightnessControlDelegate(false);
    GetController()->SetKeyboardBrightnessControlDelegate(
        scoped_ptr<KeyboardBrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_down_count());
    EXPECT_FALSE(ProcessWithContext(alt_brightness_down));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_down_count());
    EXPECT_EQ(alt_brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_up_count());
    EXPECT_FALSE(ProcessWithContext(alt_brightness_up));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_up_count());
    EXPECT_EQ(alt_brightness_up, delegate->last_accelerator());
  }
  {
    DummyKeyboardBrightnessControlDelegate* delegate =
        new DummyKeyboardBrightnessControlDelegate(true);
    GetController()->SetKeyboardBrightnessControlDelegate(
        scoped_ptr<KeyboardBrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_down_count());
    EXPECT_TRUE(ProcessWithContext(alt_brightness_down));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_down_count());
    EXPECT_EQ(alt_brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_up_count());
    EXPECT_TRUE(ProcessWithContext(alt_brightness_up));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_up_count());
    EXPECT_EQ(alt_brightness_up, delegate->last_accelerator());
  }
#endif

#if !defined(NDEBUG)
  // RotateScreen
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_HOME, ui::EF_CONTROL_DOWN)));
  // ToggleDesktopBackgroundMode
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));
#if !defined(OS_LINUX)
  // ToggleDesktopFullScreen (not implemented yet on Linux)
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_F11, ui::EF_CONTROL_DOWN)));
#endif //  OS_LINUX
#endif //  !NDEBUG

  // Exit
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // New tab
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN)));

  // New incognito window
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // New window
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN)));

  // Restore tab
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // Show task manager
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN)));

#if defined(OS_CHROMEOS)
  // Open 'open file' dialog
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_O, ui::EF_CONTROL_DOWN)));

  // Open file manager
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_M, ui::EF_CONTROL_DOWN  | ui::EF_ALT_DOWN)));

  // Lock screen
  // NOTE: Accelerators that do not work on the lock screen need to be
  // tested before the sequence below is invoked because it causes a side
  // effect of locking the screen.
  EXPECT_TRUE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
#endif
}

TEST_F(AcceleratorControllerTest, GlobalAcceleratorsToggleAppList) {
  test::TestShellDelegate* delegate =
      reinterpret_cast<test::TestShellDelegate*>(
          ash::Shell::GetInstance()->delegate());
  EXPECT_FALSE(ash::Shell::GetInstance()->GetAppListTargetVisibility());

  // The press event should not open the AppList, the release should instead.
  EXPECT_FALSE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_TRUE(ProcessWithContext(
      ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_TRUE(ash::Shell::GetInstance()->GetAppListTargetVisibility());

  // When spoken feedback is on, the AppList should not toggle.
  delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_FALSE(ProcessWithContext(
      ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(ash::Shell::GetInstance()->GetAppListTargetVisibility());

  EXPECT_FALSE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_TRUE(ProcessWithContext(
      ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_FALSE(ash::Shell::GetInstance()->GetAppListTargetVisibility());

  // When spoken feedback is on, the AppList should not toggle.
  delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(ProcessWithContext(
      ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  EXPECT_FALSE(ProcessWithContext(
      ReleaseAccelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  delegate->ToggleSpokenFeedback(A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(ash::Shell::GetInstance()->GetAppListTargetVisibility());
}

TEST_F(AcceleratorControllerTest, ImeGlobalAccelerators) {
  // Test IME shortcuts.
  {
    const ui::Accelerator control_space(ui::VKEY_SPACE, ui::EF_CONTROL_DOWN);
    const ui::Accelerator convert(ui::VKEY_CONVERT, ui::EF_NONE);
    const ui::Accelerator non_convert(ui::VKEY_NONCONVERT, ui::EF_NONE);
    const ui::Accelerator wide_half_1(ui::VKEY_DBE_SBCSCHAR, ui::EF_NONE);
    const ui::Accelerator wide_half_2(ui::VKEY_DBE_DBCSCHAR, ui::EF_NONE);
    const ui::Accelerator hangul(ui::VKEY_HANGUL, ui::EF_NONE);
    EXPECT_FALSE(ProcessWithContext(control_space));
    EXPECT_FALSE(ProcessWithContext(convert));
    EXPECT_FALSE(ProcessWithContext(non_convert));
    EXPECT_FALSE(ProcessWithContext(wide_half_1));
    EXPECT_FALSE(ProcessWithContext(wide_half_2));
    EXPECT_FALSE(ProcessWithContext(hangul));
    DummyImeControlDelegate* delegate = new DummyImeControlDelegate(true);
    GetController()->SetImeControlDelegate(
        scoped_ptr<ImeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_previous_ime_count());
    EXPECT_TRUE(ProcessWithContext(control_space));
    EXPECT_EQ(1, delegate->handle_previous_ime_count());
    EXPECT_EQ(0, delegate->handle_switch_ime_count());
    EXPECT_TRUE(ProcessWithContext(convert));
    EXPECT_EQ(1, delegate->handle_switch_ime_count());
    EXPECT_TRUE(ProcessWithContext(non_convert));
    EXPECT_EQ(2, delegate->handle_switch_ime_count());
    EXPECT_TRUE(ProcessWithContext(wide_half_1));
    EXPECT_EQ(3, delegate->handle_switch_ime_count());
    EXPECT_TRUE(ProcessWithContext(wide_half_2));
    EXPECT_EQ(4, delegate->handle_switch_ime_count());
    EXPECT_TRUE(ProcessWithContext(hangul));
    EXPECT_EQ(5, delegate->handle_switch_ime_count());
  }

  // Test IME shortcuts that are triggered on key release.
  {
    const ui::Accelerator shift_alt_press(ui::VKEY_MENU,
                                          ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    const ReleaseAccelerator shift_alt(ui::VKEY_MENU, ui::EF_SHIFT_DOWN);
    const ui::Accelerator alt_shift_press(ui::VKEY_SHIFT,
                                          ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    const ReleaseAccelerator alt_shift(ui::VKEY_SHIFT, ui::EF_ALT_DOWN);

    DummyImeControlDelegate* delegate = new DummyImeControlDelegate(true);
    GetController()->SetImeControlDelegate(
        scoped_ptr<ImeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_next_ime_count());
    EXPECT_FALSE(ProcessWithContext(shift_alt_press));
    EXPECT_TRUE(ProcessWithContext(shift_alt));
    EXPECT_EQ(1, delegate->handle_next_ime_count());
    EXPECT_FALSE(ProcessWithContext(alt_shift_press));
    EXPECT_TRUE(ProcessWithContext(alt_shift));
    EXPECT_EQ(2, delegate->handle_next_ime_count());

    // We should NOT switch IME when e.g. Shift+Alt+X is pressed and X is
    // released.
    const ui::Accelerator shift_alt_x_press(
        ui::VKEY_X,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    const ReleaseAccelerator shift_alt_x(ui::VKEY_X,
                                         ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);

    EXPECT_FALSE(ProcessWithContext(shift_alt_press));
    EXPECT_FALSE(ProcessWithContext(shift_alt_x_press));
    EXPECT_FALSE(ProcessWithContext(shift_alt_x));
    EXPECT_FALSE(ProcessWithContext(shift_alt));
    EXPECT_EQ(2, delegate->handle_next_ime_count());

    // But we _should_ if X is either VKEY_RETURN or VKEY_SPACE.
    // TODO(nona|mazda): Remove this when crbug.com/139556 in a better way.
    const ui::Accelerator shift_alt_return_press(
        ui::VKEY_RETURN,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    const ReleaseAccelerator shift_alt_return(
        ui::VKEY_RETURN,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);

    EXPECT_FALSE(ProcessWithContext(shift_alt_press));
    EXPECT_FALSE(ProcessWithContext(shift_alt_return_press));
    EXPECT_FALSE(ProcessWithContext(shift_alt_return));
    EXPECT_TRUE(ProcessWithContext(shift_alt));
    EXPECT_EQ(3, delegate->handle_next_ime_count());

    const ui::Accelerator shift_alt_space_press(
        ui::VKEY_SPACE,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    const ReleaseAccelerator shift_alt_space(
        ui::VKEY_SPACE,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);

    EXPECT_FALSE(ProcessWithContext(shift_alt_press));
    EXPECT_FALSE(ProcessWithContext(shift_alt_space_press));
    EXPECT_FALSE(ProcessWithContext(shift_alt_space));
    EXPECT_TRUE(ProcessWithContext(shift_alt));
    EXPECT_EQ(4, delegate->handle_next_ime_count());
  }

#if defined(OS_CHROMEOS)
  // Test IME shortcuts again with unnormalized accelerators (Chrome OS only).
  {
    const ui::Accelerator shift_alt_press(ui::VKEY_MENU, ui::EF_SHIFT_DOWN);
    const ReleaseAccelerator shift_alt(ui::VKEY_MENU, ui::EF_SHIFT_DOWN);
    const ui::Accelerator alt_shift_press(ui::VKEY_SHIFT, ui::EF_ALT_DOWN);
    const ReleaseAccelerator alt_shift(ui::VKEY_SHIFT, ui::EF_ALT_DOWN);

    DummyImeControlDelegate* delegate = new DummyImeControlDelegate(true);
    GetController()->SetImeControlDelegate(
        scoped_ptr<ImeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_next_ime_count());
    EXPECT_FALSE(ProcessWithContext(shift_alt_press));
    EXPECT_TRUE(ProcessWithContext(shift_alt));
    EXPECT_EQ(1, delegate->handle_next_ime_count());
    EXPECT_FALSE(ProcessWithContext(alt_shift_press));
    EXPECT_TRUE(ProcessWithContext(alt_shift));
    EXPECT_EQ(2, delegate->handle_next_ime_count());

    // We should NOT switch IME when e.g. Shift+Alt+X is pressed and X is
    // released.
    const ui::Accelerator shift_alt_x_press(
        ui::VKEY_X,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    const ReleaseAccelerator shift_alt_x(ui::VKEY_X,
                                         ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);

    EXPECT_FALSE(ProcessWithContext(shift_alt_press));
    EXPECT_FALSE(ProcessWithContext(shift_alt_x_press));
    EXPECT_FALSE(ProcessWithContext(shift_alt_x));
    EXPECT_FALSE(ProcessWithContext(shift_alt));
    EXPECT_EQ(2, delegate->handle_next_ime_count());
  }
#endif
}

// TODO(nona|mazda): Remove this when crbug.com/139556 in a better way.
TEST_F(AcceleratorControllerTest, ImeGlobalAcceleratorsWorkaround139556) {
  // The workaround for crbug.com/139556 depends on the fact that we don't
  // use Shift+Alt+Enter/Space with ET_KEY_PRESSED as an accelerator. Test it.
  const ui::Accelerator shift_alt_return_press(
      ui::VKEY_RETURN,
      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(ProcessWithContext(shift_alt_return_press));
  const ui::Accelerator shift_alt_space_press(
      ui::VKEY_SPACE,
      ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  EXPECT_FALSE(ProcessWithContext(shift_alt_space_press));
}

TEST_F(AcceleratorControllerTest, ReservedAccelerators) {
  // (Shift+)Alt+Tab and Chrome OS top-row keys are reserved.
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_POWER, ui::EF_NONE)));
#endif
  // Others are not reserved.
  EXPECT_FALSE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
  EXPECT_FALSE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE)));
  EXPECT_FALSE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_A, ui::EF_NONE)));
}

#if defined(OS_CHROMEOS)
TEST_F(AcceleratorControllerTest, DisallowedAtModalWindow) {
  std::set<AcceleratorAction> allActions;
  for (size_t i = 0 ; i < kAcceleratorDataLength; ++i)
    allActions.insert(kAcceleratorData[i].action);
  std::set<AcceleratorAction> actionsAllowedAtModalWindow;
  for (size_t k = 0 ; k < kActionsAllowedAtModalWindowLength; ++k)
    actionsAllowedAtModalWindow.insert(kActionsAllowedAtModalWindow[k]);
  for (std::set<AcceleratorAction>::const_iterator it =
           actionsAllowedAtModalWindow.begin();
       it != actionsAllowedAtModalWindow.end(); ++it) {
    EXPECT_FALSE(allActions.find(*it) == allActions.end())
        << " action from kActionsAllowedAtModalWindow"
        << " not found in kAcceleratorData. action: " << *it;
  }
  scoped_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  const ui::Accelerator dummy;
  wm::ActivateWindow(window.get());
  Shell::GetInstance()->SimulateModalWindowOpenForTesting(true);
  for (std::set<AcceleratorAction>::const_iterator it = allActions.begin();
       it != allActions.end(); ++it) {
    if (actionsAllowedAtModalWindow.find(*it) ==
        actionsAllowedAtModalWindow.end()) {
      EXPECT_TRUE(GetController()->PerformAction(*it, dummy))
          << " for action (disallowed at modal window): " << *it;
    }
  }
  //  Testing of top row (F5-F10) accelerators that should still work
  //  when a modal window is open
  //
  // Screenshot
  {
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1,
                        ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
    DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
    GetController()->SetScreenshotDelegate(
        scoped_ptr<ScreenshotDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(1, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(ProcessWithContext(
        ui::Accelerator(ui::VKEY_MEDIA_LAUNCH_APP1,
 ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
  }
  // Brightness
  const ui::Accelerator brightness_down(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  const ui::Accelerator brightness_up(ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  {
    EXPECT_FALSE(ProcessWithContext(brightness_down));
    EXPECT_FALSE(ProcessWithContext(brightness_up));
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(true);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_FALSE(ProcessWithContext(brightness_down));
    EXPECT_FALSE(ProcessWithContext(brightness_up));
  }
  EnableInternalDisplay();
  {
    EXPECT_FALSE(ProcessWithContext(brightness_down));
    EXPECT_FALSE(ProcessWithContext(brightness_up));
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(false);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_FALSE(ProcessWithContext(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_FALSE(ProcessWithContext(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(true);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_TRUE(ProcessWithContext(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_TRUE(ProcessWithContext(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }
  // Volume
  const ui::Accelerator volume_mute(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  const ui::Accelerator volume_down(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  const ui::Accelerator volume_up(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  {
    EXPECT_TRUE(ProcessWithContext(volume_mute));
    EXPECT_TRUE(ProcessWithContext(volume_down));
    EXPECT_TRUE(ProcessWithContext(volume_up));
    DummyVolumeControlDelegate* delegate =
        new DummyVolumeControlDelegate(false);
    ash::Shell::GetInstance()->system_tray_delegate()->SetVolumeControlDelegate(
        scoped_ptr<VolumeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_volume_mute_count());
    EXPECT_FALSE(ProcessWithContext(volume_mute));
    EXPECT_EQ(1, delegate->handle_volume_mute_count());
    EXPECT_EQ(volume_mute, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_down_count());
    EXPECT_FALSE(ProcessWithContext(volume_down));
    EXPECT_EQ(1, delegate->handle_volume_down_count());
    EXPECT_EQ(volume_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_up_count());
    EXPECT_FALSE(ProcessWithContext(volume_up));
    EXPECT_EQ(1, delegate->handle_volume_up_count());
    EXPECT_EQ(volume_up, delegate->last_accelerator());
  }
  {
    DummyVolumeControlDelegate* delegate = new DummyVolumeControlDelegate(true);
    ash::Shell::GetInstance()->system_tray_delegate()->SetVolumeControlDelegate(
        scoped_ptr<VolumeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_volume_mute_count());
    EXPECT_TRUE(ProcessWithContext(volume_mute));
    EXPECT_EQ(1, delegate->handle_volume_mute_count());
    EXPECT_EQ(volume_mute, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_down_count());
    EXPECT_TRUE(ProcessWithContext(volume_down));
    EXPECT_EQ(1, delegate->handle_volume_down_count());
    EXPECT_EQ(volume_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_up_count());
    EXPECT_TRUE(ProcessWithContext(volume_up));
    EXPECT_EQ(1, delegate->handle_volume_up_count());
    EXPECT_EQ(volume_up, delegate->last_accelerator());
  }
}
#endif

}  // namespace ash
