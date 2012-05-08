// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_controller.h"
#include "ash/caps_lock_delegate.h"
#include "ash/ime_control_delegate.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/volume_control_delegate.h"
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

class ReleaseAccelerator : public ui::Accelerator {
 public:
  ReleaseAccelerator(ui::KeyboardCode keycode,
                     bool shift_pressed,
                     bool ctrl_pressed,
                     bool alt_pressed)
      : ui::Accelerator(keycode, shift_pressed, ctrl_pressed, alt_pressed) {
    set_type(ui::ET_KEY_RELEASED);
  }
};

class DummyScreenshotDelegate : public ScreenshotDelegate {
 public:
  DummyScreenshotDelegate()
      : handle_take_screenshot_count_(0),
        handle_take_partial_screenshot_count_(0) {
  }
  virtual ~DummyScreenshotDelegate() {}

  // Overridden from ScreenshotDelegate:
  virtual void HandleTakeScreenshot(aura::Window* window) OVERRIDE {
    if (window != NULL)
      ++handle_take_screenshot_count_;
  }

  virtual void HandleTakePartialScreenshot(
      aura::Window* window, const gfx::Rect& rect) OVERRIDE {
    ++handle_take_partial_screenshot_count_;
  }

  int handle_take_screenshot_count() const {
    return handle_take_screenshot_count_;
  }

  int handle_take_partial_screenshot_count() const {
    return handle_take_partial_screenshot_count_;
  }

 private:
  int handle_take_screenshot_count_;
  int handle_take_partial_screenshot_count_;

  DISALLOW_COPY_AND_ASSIGN(DummyScreenshotDelegate);
};

class DummyCapsLockDelegate : public CapsLockDelegate {
 public:
  explicit DummyCapsLockDelegate(bool consume)
      : consume_(consume),
        handle_caps_lock_count_(0) {
  }
  virtual ~DummyCapsLockDelegate() {}

  virtual bool HandleToggleCapsLock() OVERRIDE {
    ++handle_caps_lock_count_;
    return consume_;
  }

  int handle_caps_lock_count() const {
    return handle_caps_lock_count_;
  }

 private:
  const bool consume_;
  int handle_caps_lock_count_;

  DISALLOW_COPY_AND_ASSIGN(DummyCapsLockDelegate);
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

 private:
  const bool consume_;
  int handle_next_ime_count_;
  int handle_previous_ime_count_;
  int handle_switch_ime_count_;
  ui::Accelerator last_accelerator_;

  DISALLOW_COPY_AND_ASSIGN(DummyImeControlDelegate);
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

TEST_F(AcceleratorControllerTest, IsRegistered) {
  const ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  const ui::Accelerator accelerator_shift_a(ui::VKEY_A, true, false, false);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);
  EXPECT_TRUE(GetController()->IsRegistered(accelerator_a));
  EXPECT_FALSE(GetController()->IsRegistered(accelerator_shift_a));
  GetController()->UnregisterAll(&target);
  EXPECT_FALSE(GetController()->IsRegistered(accelerator_a));
}

#if defined(OS_WIN) || defined(USE_X11)
TEST_F(AcceleratorControllerTest, ProcessOnce) {
  ui::Accelerator accelerator_a(ui::VKEY_A, false, false, false);
  TestTarget target;
  GetController()->Register(accelerator_a, &target);

  // The accelerator is processed only once.
#if defined(OS_WIN)
  MSG msg1 = { NULL, WM_KEYDOWN, ui::VKEY_A, 0 };
  aura::TranslatedKeyEvent key_event1(msg1, false);
  EXPECT_TRUE(Shell::GetRootWindow()->DispatchKeyEvent(&key_event1));

  MSG msg2 = { NULL, WM_CHAR, L'A', 0 };
  aura::TranslatedKeyEvent key_event2(msg2, true);
  EXPECT_FALSE(Shell::GetRootWindow()->DispatchKeyEvent(&key_event2));

  MSG msg3 = { NULL, WM_KEYUP, ui::VKEY_A, 0 };
  aura::TranslatedKeyEvent key_event3(msg3, false);
  EXPECT_FALSE(Shell::GetRootWindow()->DispatchKeyEvent(&key_event3));
#elif defined(USE_X11)
  XEvent key_event;
  ui::InitXKeyEventForTesting(ui::ET_KEY_PRESSED,
                              ui::VKEY_A,
                              0,
                              &key_event);
  aura::TranslatedKeyEvent key_event1(&key_event, false);
  EXPECT_TRUE(Shell::GetRootWindow()->DispatchKeyEvent(&key_event1));

  aura::TranslatedKeyEvent key_event2(&key_event, true);
  EXPECT_FALSE(Shell::GetRootWindow()->DispatchKeyEvent(&key_event2));

  ui::InitXKeyEventForTesting(ui::ET_KEY_RELEASED,
                              ui::VKEY_A,
                              0,
                              &key_event);
  aura::TranslatedKeyEvent key_event3(&key_event, false);
  EXPECT_FALSE(Shell::GetRootWindow()->DispatchKeyEvent(&key_event3));
#endif
  EXPECT_EQ(1, target.accelerator_pressed_count());
}
#endif

TEST_F(AcceleratorControllerTest, GlobalAccelerators) {
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
  // Take screenshot / partial screenshot
  // True should always be returned regardless of the existence of the delegate.
  {
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_F5, false, true, false)));
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_PRINT, false, false, false)));
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_F5, true, true, false)));
    DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
    GetController()->SetScreenshotDelegate(
        scoped_ptr<ScreenshotDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_take_screenshot_count());
    EXPECT_EQ(0, delegate->handle_take_partial_screenshot_count());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_F5, false, true, false)));
    EXPECT_EQ(1, delegate->handle_take_screenshot_count());
    EXPECT_EQ(0, delegate->handle_take_partial_screenshot_count());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_PRINT, false, false, false)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
    EXPECT_EQ(0, delegate->handle_take_partial_screenshot_count());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_F5, true, true, false)));
    EXPECT_EQ(2, delegate->handle_take_screenshot_count());
    EXPECT_EQ(1, delegate->handle_take_partial_screenshot_count());
  }
  // ToggleAppList
  {
    EXPECT_FALSE(ash::Shell::GetInstance()->GetAppListTargetVisibility());
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, false, true, false)));
    EXPECT_TRUE(ash::Shell::GetInstance()->GetAppListTargetVisibility());
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, false, true, false)));
    EXPECT_FALSE(ash::Shell::GetInstance()->GetAppListTargetVisibility());
  }
  // ToggleCapsLock
  {
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, true, false, false)));
    DummyCapsLockDelegate* delegate = new DummyCapsLockDelegate(false);
    GetController()->SetCapsLockDelegate(
        scoped_ptr<CapsLockDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_caps_lock_count());
    EXPECT_FALSE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, true, false, false)));
    EXPECT_EQ(1, delegate->handle_caps_lock_count());
  }
  {
    DummyCapsLockDelegate* delegate = new DummyCapsLockDelegate(true);
    GetController()->SetCapsLockDelegate(
        scoped_ptr<CapsLockDelegate>(delegate).Pass());
    EXPECT_EQ(0, delegate->handle_caps_lock_count());
    EXPECT_TRUE(GetController()->Process(
        ui::Accelerator(ui::VKEY_LWIN, true, false, false)));
    EXPECT_EQ(1, delegate->handle_caps_lock_count());
  }
  // Volume
  const ui::Accelerator f8(ui::VKEY_F8, false, false, false);
  const ui::Accelerator f9(ui::VKEY_F9, false, false, false);
  const ui::Accelerator f10(ui::VKEY_F10, false, false, false);
  {
    EXPECT_FALSE(GetController()->Process(f8));
    EXPECT_FALSE(GetController()->Process(f9));
    EXPECT_FALSE(GetController()->Process(f10));
    DummyVolumeControlDelegate* delegate =
        new DummyVolumeControlDelegate(false);
    GetController()->SetVolumeControlDelegate(
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
    GetController()->SetVolumeControlDelegate(
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
  const ui::Accelerator volume_mute(
      ui::VKEY_VOLUME_MUTE, false, false, false);
  const ui::Accelerator volume_down(
      ui::VKEY_VOLUME_DOWN, false, false, false);
  const ui::Accelerator volume_up(ui::VKEY_VOLUME_UP, false, false, false);
  {
    DummyVolumeControlDelegate* delegate =
        new DummyVolumeControlDelegate(false);
    GetController()->SetVolumeControlDelegate(
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
    GetController()->SetVolumeControlDelegate(
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
  const ui::Accelerator f6(ui::VKEY_F6, false, false, false);
  const ui::Accelerator f7(ui::VKEY_F7, false, false, false);
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
  const ui::Accelerator brightness_down(
      ui::VKEY_BRIGHTNESS_DOWN, false, false, false);
  const ui::Accelerator brightness_up(
      ui::VKEY_BRIGHTNESS_UP, false, false, false);
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
#if !defined(NDEBUG)
  // RotateScreen
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_HOME, false, true, false)));
  // ToggleDesktopBackgroundMode
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_B, false, true, true)));
#if !defined(OS_LINUX)
  // ToggleDesktopFullScreen (not implemented yet on Linux)
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_F11, false, true, false)));
#endif //  OS_LINUX
#endif //  !NDEBUG

  // Exit
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_Q, true, true ,false)));

  // New incognito window
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_N, true, true, false)));

  // New window
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_N, false, true, false)));

#if defined(OS_CHROMEOS)
  EXPECT_TRUE(GetController()->Process(
      ui::Accelerator(ui::VKEY_L, true, true, false)));
#endif
}

TEST_F(AcceleratorControllerTest, ImeGlobalAccelerators) {
  // Test IME shortcuts.
  {
    const ui::Accelerator control_space(ui::VKEY_SPACE, false, true, false);
    const ui::Accelerator convert(ui::VKEY_CONVERT, false, false, false);
    const ui::Accelerator non_convert(ui::VKEY_NONCONVERT, false, false, false);
    const ui::Accelerator wide_half_1(
        ui::VKEY_DBE_SBCSCHAR, false, false, false);
    const ui::Accelerator wide_half_2(
        ui::VKEY_DBE_DBCSCHAR, false, false, false);
    const ui::Accelerator hangul(ui::VKEY_HANGUL, false, false, false);
    const ui::Accelerator shift_space(ui::VKEY_SPACE, true, false, false);
    EXPECT_FALSE(GetController()->Process(control_space));
    EXPECT_FALSE(GetController()->Process(convert));
    EXPECT_FALSE(GetController()->Process(non_convert));
    EXPECT_FALSE(GetController()->Process(wide_half_1));
    EXPECT_FALSE(GetController()->Process(wide_half_2));
    EXPECT_FALSE(GetController()->Process(hangul));
    EXPECT_FALSE(GetController()->Process(shift_space));
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
    EXPECT_TRUE(GetController()->Process(shift_space));
    EXPECT_EQ(6, delegate->handle_switch_ime_count());
  }

  // Test IME shortcuts that are triggered on key release.
  {
    const ui::Accelerator shift_alt_press(ui::VKEY_MENU, true, false, true);
    const ReleaseAccelerator shift_alt(ui::VKEY_MENU, true, false, true);
    const ui::Accelerator alt_shift_press(ui::VKEY_SHIFT, true, false, true);
    const ReleaseAccelerator alt_shift(ui::VKEY_SHIFT, true, false, true);

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
    const ui::Accelerator shift_alt_x_press(ui::VKEY_X, true, false, true);
    const ReleaseAccelerator shift_alt_x(ui::VKEY_X, true, false, true);

    EXPECT_FALSE(GetController()->Process(shift_alt_press));
    EXPECT_FALSE(GetController()->Process(shift_alt_x_press));
    EXPECT_FALSE(GetController()->Process(shift_alt_x));
    EXPECT_FALSE(GetController()->Process(shift_alt));
    EXPECT_EQ(2, delegate->handle_next_ime_count());
  }
}

}  // namespace test
}  // namespace ash
