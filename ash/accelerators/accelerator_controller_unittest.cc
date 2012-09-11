// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/caps_lock_delegate.h"
#include "ash/display/multi_display_manager.h"
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
#include "ui/aura/env.h"
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
  ui::Accelerator RemapAccelerator(
      const ui::Accelerator& accelerator) {
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

class AcceleratorControllerTest : public AshTestBase {
 public:
  AcceleratorControllerTest() {};
  virtual ~AcceleratorControllerTest() {};

 protected:
  void EnableInternalDisplay() {
    static_cast<internal::MultiDisplayManager*>(
        aura::Env::GetInstance()->display_manager())->
        EnableInternalDisplayForTest();
  }

  static AcceleratorController* GetController();

 private:
  DISALLOW_COPY_AND_ASSIGN(AcceleratorControllerTest);
};

AcceleratorController* AcceleratorControllerTest::GetController() {
  return Shell::GetInstance()->accelerator_controller();
}

TEST_F(AcceleratorControllerTest, Register) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);

  // The registered accelerator is processed.
  EXPECT_TRUE(GetController()->Process(accelerator_a));
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
  EXPECT_TRUE(GetController()->Process(accelerator_a));
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
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_EQ(1, target.accelerator_pressed_count());

  // The unregistered accelerator is no longer processed.
  target.set_accelerator_pressed_count(0);
  GetController()->Unregister(accelerator_a, &target);
  EXPECT_FALSE(GetController()->Process(accelerator_a));
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
  EXPECT_FALSE(GetController()->Process(accelerator_a));
  EXPECT_FALSE(GetController()->Process(accelerator_b));
  EXPECT_EQ(0, target1.accelerator_pressed_count());

  // UnregisterAll with a different target does not affect the other target.
  EXPECT_TRUE(GetController()->Process(accelerator_c));
  EXPECT_EQ(1, target2.accelerator_pressed_count());
}

TEST_F(AcceleratorControllerTest, Process) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, ui::EF_NONE);
  TestTarget target1;
  GetController()->Register(accelerator_a, &target1);

  // The registered accelerator is processed.
  EXPECT_TRUE(GetController()->Process(accelerator_a));
  EXPECT_EQ(1, target1.accelerator_pressed_count());

  // The non-registered accelerator is not processed.
  const ui::Accelerator accelerator_b(ui::VKEY_B, ui::EF_NONE);
  EXPECT_FALSE(GetController()->Process(accelerator_b));
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
      aura::test::CreateTestWindowWithBounds(gfx::Rect(5, 5, 20, 20), NULL));
  const ui::Accelerator dummy;

  wm::ActivateWindow(window.get());

  {
    GetController()->PerformAction(WINDOW_SNAP_LEFT, dummy);
    gfx::Rect snap_left = window->bounds();
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

    // It should cycle back to the first snapped position.
    GetController()->PerformAction(WINDOW_SNAP_RIGHT, dummy);
    EXPECT_EQ(window->bounds().ToString(), snap_right.ToString());
  }
  {
    gfx::Rect normal_bounds = window->bounds();

    GetController()->PerformAction(WINDOW_MAXIMIZE_RESTORE, dummy);
    EXPECT_TRUE(wm::IsWindowMaximized(window.get()));
    EXPECT_NE(normal_bounds.ToString(), window->bounds().ToString());

    GetController()->PerformAction(WINDOW_MAXIMIZE_RESTORE, dummy);
    EXPECT_FALSE(wm::IsWindowMaximized(window.get()));
    EXPECT_EQ(normal_bounds.ToString(), window->bounds().ToString());

    GetController()->PerformAction(WINDOW_MAXIMIZE_RESTORE, dummy);
    GetController()->PerformAction(WINDOW_SNAP_LEFT, dummy);
    EXPECT_FALSE(wm::IsWindowMaximized(window.get()));

    GetController()->PerformAction(WINDOW_MAXIMIZE_RESTORE, dummy);
    GetController()->PerformAction(WINDOW_SNAP_RIGHT, dummy);
    EXPECT_FALSE(wm::IsWindowMaximized(window.get()));

    GetController()->PerformAction(WINDOW_MAXIMIZE_RESTORE, dummy);
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
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F5, ui::EF_SHIFT_DOWN)));
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
  // CycleForward
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F5, ui::EF_NONE)));
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  // Take screenshot / partial screenshot
  // True should always be returned regardless of the existence of the delegate.
  {
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_F5, ui::EF_CONTROL_DOWN)));
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_F5, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
    DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
    GetController()->SetScreenshotDelegate(
        scoped_ptr<ScreenshotDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_F5, ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(1, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_F5, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
  }
  // ToggleAppList
  {
    EXPECT_FALSE(ash::Shell::GetInstance()->GetAppListTargetVisibility());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
    EXPECT_TRUE(ash::Shell::GetInstance()->GetAppListTargetVisibility());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
    EXPECT_FALSE(ash::Shell::GetInstance()->GetAppListTargetVisibility());
  }
  // ToggleAppList (with spoken feedback enabled)
  {
    ShellDelegate* delegate = ash::Shell::GetInstance()->delegate();
    delegate->ToggleSpokenFeedback();
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
    delegate->ToggleSpokenFeedback();
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_NONE)));
  }
  // DisableCapsLock
  {
    CapsLockDelegate* delegate = Shell::GetInstance()->caps_lock_delegate();
    delegate->SetCapsLockEnabled(true);
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    // Handled only on key release.
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LSHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_TRUE(GetController()->Process(
        ReleaseAccelerator(ui::VKEY_SHIFT, ui::EF_NONE)));
    EXPECT_FALSE(delegate->IsCapsLockEnabled());
    delegate->SetCapsLockEnabled(true);
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_RSHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_TRUE(GetController()->Process(
        ReleaseAccelerator(ui::VKEY_LSHIFT, ui::EF_NONE)));
    EXPECT_FALSE(delegate->IsCapsLockEnabled());
    delegate->SetCapsLockEnabled(true);
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_SHIFT, ui::EF_NONE)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_TRUE(GetController()->Process(
        ReleaseAccelerator(ui::VKEY_RSHIFT, ui::EF_NONE)));
    EXPECT_FALSE(delegate->IsCapsLockEnabled());

    // Do not handle when a shift pressed with other keys.
    delegate->SetCapsLockEnabled(true);
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_A, ui::EF_SHIFT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_FALSE(GetController()->Process(
        ReleaseAccelerator(ui::VKEY_A, ui::EF_SHIFT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
  }
  // ToggleCapsLock
  {
    CapsLockDelegate* delegate = Shell::GetInstance()->caps_lock_delegate();
    delegate->SetCapsLockEnabled(true);
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_ALT_DOWN)));
    EXPECT_FALSE(delegate->IsCapsLockEnabled());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, ui::EF_ALT_DOWN)));
    EXPECT_TRUE(delegate->IsCapsLockEnabled());
  }
  // Volume
  const ui::Accelerator f8(ui::VKEY_F8, ui::EF_NONE);
  const ui::Accelerator f9(ui::VKEY_F9, ui::EF_NONE);
  const ui::Accelerator f10(ui::VKEY_F10, ui::EF_NONE);
  {
    EXPECT_TRUE(GetController()->Process(f8));
    EXPECT_TRUE(GetController()->Process(f9));
    EXPECT_TRUE(GetController()->Process(f10));
    DummyVolumeControlDelegate* delegate =
        new DummyVolumeControlDelegate(false);
    ash::Shell::GetInstance()->tray_delegate()->SetVolumeControlDelegate(
        scoped_ptr<VolumeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_volume_mute_count());
    EXPECT_FALSE(GetController()->Process(f8));
    EXPECT_EQ(1, delegate->handle_volume_mute_count());
    EXPECT_EQ(f8, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_down_count());
    EXPECT_FALSE(GetController()->Process(f9));
    EXPECT_EQ(1, delegate->handle_volume_down_count());
    EXPECT_EQ(f9, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_up_count());
    EXPECT_FALSE(GetController()->Process(f10));
    EXPECT_EQ(1, delegate->handle_volume_up_count());
    EXPECT_EQ(f10, delegate->last_accelerator());
  }
  {
    DummyVolumeControlDelegate* delegate = new DummyVolumeControlDelegate(true);
    ash::Shell::GetInstance()->tray_delegate()->SetVolumeControlDelegate(
        scoped_ptr<VolumeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_volume_mute_count());
    EXPECT_TRUE(GetController()->Process(f8));
    EXPECT_EQ(1, delegate->handle_volume_mute_count());
    EXPECT_EQ(f8, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_down_count());
    EXPECT_TRUE(GetController()->Process(f9));
    EXPECT_EQ(1, delegate->handle_volume_down_count());
    EXPECT_EQ(f9, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_up_count());
    EXPECT_TRUE(GetController()->Process(f10));
    EXPECT_EQ(1, delegate->handle_volume_up_count());
    EXPECT_EQ(f10, delegate->last_accelerator());
  }
  const ui::Accelerator volume_mute(ui::VKEY_VOLUME_MUTE, ui::EF_NONE);
  const ui::Accelerator volume_down(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  const ui::Accelerator volume_up(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  {
    DummyVolumeControlDelegate* delegate =
        new DummyVolumeControlDelegate(false);
    ash::Shell::GetInstance()->tray_delegate()->SetVolumeControlDelegate(
        scoped_ptr<VolumeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_volume_mute_count());
    EXPECT_FALSE(GetController()->Process(volume_mute));
    EXPECT_EQ(1, delegate->handle_volume_mute_count());
    EXPECT_EQ(volume_mute, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_down_count());
    EXPECT_FALSE(GetController()->Process(volume_down));
    EXPECT_EQ(1, delegate->handle_volume_down_count());
    EXPECT_EQ(volume_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_up_count());
    EXPECT_FALSE(GetController()->Process(volume_up));
    EXPECT_EQ(1, delegate->handle_volume_up_count());
    EXPECT_EQ(volume_up, delegate->last_accelerator());
  }
  {
    DummyVolumeControlDelegate* delegate = new DummyVolumeControlDelegate(true);
    ash::Shell::GetInstance()->tray_delegate()->SetVolumeControlDelegate(
        scoped_ptr<VolumeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_volume_mute_count());
    EXPECT_TRUE(GetController()->Process(volume_mute));
    EXPECT_EQ(1, delegate->handle_volume_mute_count());
    EXPECT_EQ(volume_mute, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_down_count());
    EXPECT_TRUE(GetController()->Process(volume_down));
    EXPECT_EQ(1, delegate->handle_volume_down_count());
    EXPECT_EQ(volume_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_volume_up_count());
    EXPECT_TRUE(GetController()->Process(volume_up));
    EXPECT_EQ(1, delegate->handle_volume_up_count());
    EXPECT_EQ(volume_up, delegate->last_accelerator());
  }
  // Brightness
  const ui::Accelerator f6(ui::VKEY_F6, ui::EF_NONE);
  const ui::Accelerator f7(ui::VKEY_F7, ui::EF_NONE);
  {
    EXPECT_FALSE(GetController()->Process(f6));
    EXPECT_FALSE(GetController()->Process(f7));
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(true);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_FALSE(GetController()->Process(f6));
    EXPECT_FALSE(GetController()->Process(f7));
  }
  // Enable internal display.
  EnableInternalDisplay();
  {
    EXPECT_FALSE(GetController()->Process(f6));
    EXPECT_FALSE(GetController()->Process(f7));
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(false);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_FALSE(GetController()->Process(f6));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(f6, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_FALSE(GetController()->Process(f7));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(f7, delegate->last_accelerator());
  }
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(true);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_TRUE(GetController()->Process(f6));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(f6, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_TRUE(GetController()->Process(f7));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(f7, delegate->last_accelerator());
  }
#if defined(OS_CHROMEOS)
  // ui::VKEY_BRIGHTNESS_DOWN/UP are not defined on Windows.
  const ui::Accelerator brightness_down(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  const ui::Accelerator brightness_up(ui::VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(false);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_FALSE(GetController()->Process(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_FALSE(GetController()->Process(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }
  {
    DummyBrightnessControlDelegate* delegate =
        new DummyBrightnessControlDelegate(true);
    GetController()->SetBrightnessControlDelegate(
        scoped_ptr<BrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_brightness_down_count());
    EXPECT_TRUE(GetController()->Process(brightness_down));
    EXPECT_EQ(1, delegate->handle_brightness_down_count());
    EXPECT_EQ(brightness_down, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_brightness_up_count());
    EXPECT_TRUE(GetController()->Process(brightness_up));
    EXPECT_EQ(1, delegate->handle_brightness_up_count());
    EXPECT_EQ(brightness_up, delegate->last_accelerator());
  }
#endif

  // Keyboard brightness
  const ui::Accelerator alt_f6(ui::VKEY_F6, ui::EF_ALT_DOWN);
  const ui::Accelerator alt_f7(ui::VKEY_F7, ui::EF_ALT_DOWN);
  {
    EXPECT_FALSE(GetController()->Process(alt_f6));
    EXPECT_FALSE(GetController()->Process(alt_f7));
    DummyKeyboardBrightnessControlDelegate* delegate =
        new DummyKeyboardBrightnessControlDelegate(false);
    GetController()->SetKeyboardBrightnessControlDelegate(
        scoped_ptr<KeyboardBrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_down_count());
    EXPECT_FALSE(GetController()->Process(alt_f6));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_down_count());
    EXPECT_EQ(alt_f6, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_up_count());
    EXPECT_FALSE(GetController()->Process(alt_f7));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_up_count());
    EXPECT_EQ(alt_f7, delegate->last_accelerator());
  }
  {
    DummyKeyboardBrightnessControlDelegate* delegate =
        new DummyKeyboardBrightnessControlDelegate(true);
    GetController()->SetKeyboardBrightnessControlDelegate(
        scoped_ptr<KeyboardBrightnessControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_down_count());
    EXPECT_TRUE(GetController()->Process(alt_f6));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_down_count());
    EXPECT_EQ(alt_f6, delegate->last_accelerator());
    EXPECT_EQ(0, delegate->handle_keyboard_brightness_up_count());
    EXPECT_TRUE(GetController()->Process(alt_f7));
    EXPECT_EQ(1, delegate->handle_keyboard_brightness_up_count());
    EXPECT_EQ(alt_f7, delegate->last_accelerator());
  }

#if !defined(NDEBUG)
  // RotateScreen
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_HOME, ui::EF_CONTROL_DOWN)));
  // ToggleDesktopBackgroundMode
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_B, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN)));
#if !defined(OS_LINUX)
  // ToggleDesktopFullScreen (not implemented yet on Linux)
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F11, ui::EF_CONTROL_DOWN)));
#endif //  OS_LINUX
#endif //  !NDEBUG

  // Exit
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_Q, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // New tab
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_T, ui::EF_CONTROL_DOWN)));

  // New incognito window
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_N, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // New window
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_N, ui::EF_CONTROL_DOWN)));

  // Restore tab
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_T, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));

  // Show task manager
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_SHIFT_DOWN)));

#if defined(OS_CHROMEOS)
  // Open file manager dialog
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_O, ui::EF_CONTROL_DOWN)));

  // Open file manager tab
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_M, ui::EF_CONTROL_DOWN)));

  // Lock screen
  // NOTE: Accelerators that do not work on the lock screen need to be
  // tested before the sequence below is invoked because it causes a side
  // effect of locking the screen.
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_L, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)));
#endif
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
    EXPECT_FALSE(GetController()->Process(control_space));
    EXPECT_FALSE(GetController()->Process(convert));
    EXPECT_FALSE(GetController()->Process(non_convert));
    EXPECT_FALSE(GetController()->Process(wide_half_1));
    EXPECT_FALSE(GetController()->Process(wide_half_2));
    EXPECT_FALSE(GetController()->Process(hangul));
    DummyImeControlDelegate* delegate = new DummyImeControlDelegate(true);
    GetController()->SetImeControlDelegate(
        scoped_ptr<ImeControlDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_previous_ime_count());
    EXPECT_TRUE(GetController()->Process(control_space));
    EXPECT_EQ(1, delegate->handle_previous_ime_count());
    EXPECT_EQ(0, delegate->handle_switch_ime_count());
    EXPECT_TRUE(GetController()->Process(convert));
    EXPECT_EQ(1, delegate->handle_switch_ime_count());
    EXPECT_TRUE(GetController()->Process(non_convert));
    EXPECT_EQ(2, delegate->handle_switch_ime_count());
    EXPECT_TRUE(GetController()->Process(wide_half_1));
    EXPECT_EQ(3, delegate->handle_switch_ime_count());
    EXPECT_TRUE(GetController()->Process(wide_half_2));
    EXPECT_EQ(4, delegate->handle_switch_ime_count());
    EXPECT_TRUE(GetController()->Process(hangul));
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
    EXPECT_FALSE(GetController()->Process(shift_alt_press));
    EXPECT_TRUE(GetController()->Process(shift_alt));
    EXPECT_EQ(1, delegate->handle_next_ime_count());
    EXPECT_FALSE(GetController()->Process(alt_shift_press));
    EXPECT_TRUE(GetController()->Process(alt_shift));
    EXPECT_EQ(2, delegate->handle_next_ime_count());

    // We should NOT switch IME when e.g. Shift+Alt+X is pressed and X is
    // released.
    const ui::Accelerator shift_alt_x_press(
        ui::VKEY_X,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    const ReleaseAccelerator shift_alt_x(ui::VKEY_X,
                                         ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);

    EXPECT_FALSE(GetController()->Process(shift_alt_press));
    EXPECT_FALSE(GetController()->Process(shift_alt_x_press));
    EXPECT_FALSE(GetController()->Process(shift_alt_x));
    EXPECT_FALSE(GetController()->Process(shift_alt));
    EXPECT_EQ(2, delegate->handle_next_ime_count());
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
    EXPECT_FALSE(GetController()->Process(shift_alt_press));
    EXPECT_TRUE(GetController()->Process(shift_alt));
    EXPECT_EQ(1, delegate->handle_next_ime_count());
    EXPECT_FALSE(GetController()->Process(alt_shift_press));
    EXPECT_TRUE(GetController()->Process(alt_shift));
    EXPECT_EQ(2, delegate->handle_next_ime_count());

    // We should NOT switch IME when e.g. Shift+Alt+X is pressed and X is
    // released.
    const ui::Accelerator shift_alt_x_press(
        ui::VKEY_X,
        ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
    const ReleaseAccelerator shift_alt_x(ui::VKEY_X,
                                         ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);

    EXPECT_FALSE(GetController()->Process(shift_alt_press));
    EXPECT_FALSE(GetController()->Process(shift_alt_x_press));
    EXPECT_FALSE(GetController()->Process(shift_alt_x));
    EXPECT_FALSE(GetController()->Process(shift_alt));
    EXPECT_EQ(2, delegate->handle_next_ime_count());
  }
#endif
}

TEST_F(AcceleratorControllerTest, ReservedAccelerators) {
  // (Shift+)Alt+Tab and Chrome OS top-row keys are reserved.
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_ALT_DOWN)));
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN)));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_F5, ui::EF_NONE)));
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_F6, ui::EF_NONE)));
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_F7, ui::EF_NONE)));
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_F8, ui::EF_NONE)));
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_F9, ui::EF_NONE)));
  EXPECT_TRUE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_F10, ui::EF_NONE)));
#endif
  // Others are not reserved.
  EXPECT_FALSE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_PRINT, ui::EF_NONE)));
  EXPECT_FALSE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_TAB, ui::EF_NONE)));
  EXPECT_FALSE(GetController()->IsReservedAccelerator(
      ui::Accelerator(ui::VKEY_A, ui::EF_NONE)));
}

}  // namespace test
}  // namespace ash
