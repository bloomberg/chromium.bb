// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/system_gesture_event_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_model.h"
#include "ash/shell.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/volume_control_delegate.h"
#include "ash/wm/window_util.h"
#include "base/time.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_windows.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace test {

namespace {

class DelegatePercentTracker {
 public:
  explicit DelegatePercentTracker()
      : handle_percent_count_(0),
        handle_percent_(0){
  }
  int handle_percent_count() const {
    return handle_percent_count_;
  }
  double handle_percent() const {
    return handle_percent_;
  }
  void SetPercent(double percent) {
    handle_percent_ = percent;
    handle_percent_count_++;
  }

 private:
  int handle_percent_count_;
  int handle_percent_;

  DISALLOW_COPY_AND_ASSIGN(DelegatePercentTracker);
};

class DummyVolumeControlDelegate : public VolumeControlDelegate,
                                   public DelegatePercentTracker {
 public:
  explicit DummyVolumeControlDelegate() {}
  virtual ~DummyVolumeControlDelegate() {}

  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual void SetVolumePercent(double percent) OVERRIDE {
    SetPercent(percent);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyVolumeControlDelegate);
};

class DummyBrightnessControlDelegate : public BrightnessControlDelegate,
                                       public DelegatePercentTracker {
 public:
  explicit DummyBrightnessControlDelegate() {}
  virtual ~DummyBrightnessControlDelegate() {}

  virtual bool HandleBrightnessDown(
      const ui::Accelerator& accelerator) OVERRIDE { return true; }
  virtual bool HandleBrightnessUp(
      const ui::Accelerator& accelerator) OVERRIDE { return true; }
  virtual void SetBrightnessPercent(double percent, bool gradual) OVERRIDE {
    SetPercent(percent);
  }
  virtual void GetBrightnessPercent(
      const base::Callback<void(double)>& callback) OVERRIDE {
    callback.Run(100.0);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyBrightnessControlDelegate);
};

} // namespace

typedef test::AshTestBase SystemGestureEventFilterTest;

// Ensure that events targeted at the root window are consumed by the
// system event handler.
TEST_F(SystemGestureEventFilterTest, TapOutsideRootWindow) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();

  Shell::TestApi shell_test(Shell::GetInstance());

  const int kTouchId = 5;

  // A touch outside the root window will be associated with the root window
  aura::TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(-10, -10), kTouchId,
      base::Time::NowFromSystemTime() - base::Time());
  root_window->DispatchTouchEvent(&press);

  aura::GestureEvent* event = new aura::GestureEvent(
      ui::ET_GESTURE_TAP, 0, 0, 0, base::Time::Now(), 0, 0, 1 << kTouchId);
  bool consumed = root_window->DispatchGestureEvent(event);

  EXPECT_TRUE(consumed);

  // Without the event filter, the touch shouldn't be consumed by the
  // system event handler.
  Shell::GetInstance()->RemoveEnvEventFilter(
      shell_test.system_gesture_event_filter());

  aura::GestureEvent* event2 = new aura::GestureEvent(
      ui::ET_GESTURE_TAP, 0, 0, 0, base::Time::Now(), 0, 0, 1 << kTouchId);
  consumed = root_window->DispatchGestureEvent(event2);

  // The event filter doesn't exist, so the touch won't be consumed.
  EXPECT_FALSE(consumed);
}

// Ensure that the device control operation gets properly handled.
TEST_F(SystemGestureEventFilterTest, DeviceControl) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();

  gfx::Rect screen = gfx::Screen::GetPrimaryDisplay().bounds();
  int ypos_half = screen.height() / 2;

  ash::AcceleratorController* accelerator =
       ash::Shell::GetInstance()->accelerator_controller();

  DummyBrightnessControlDelegate* delegateBrightness =
      new DummyBrightnessControlDelegate();
  accelerator->SetBrightnessControlDelegate(
      scoped_ptr<BrightnessControlDelegate>(delegateBrightness).Pass());

  DummyVolumeControlDelegate* delegateVolume =
      new DummyVolumeControlDelegate();
  accelerator->SetVolumeControlDelegate(
      scoped_ptr<VolumeControlDelegate>(delegateVolume).Pass());

  const int kTouchId = 5;

  for (int pass = 0; pass < 2; pass++) {
    DelegatePercentTracker* delegate =
        static_cast<DelegatePercentTracker*>(delegateBrightness);
    int xpos = screen.x() - 10;
    int ypos = screen.y();
    if (pass) {
      // On the second pass the volume will be tested.
      delegate = static_cast<DelegatePercentTracker*>(delegateVolume);
      xpos = screen.right() + 40;  // Make sure it is out of the screen.
    }
    // Get a target for kTouchId
    aura::TouchEvent press1(ui::ET_TOUCH_PRESSED,
                            gfx::Point(-10, ypos + ypos_half),
                            kTouchId,
                            base::Time::NowFromSystemTime() - base::Time());
    root_window->DispatchTouchEvent(&press1);

    aura::GestureEvent* event1 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_BEGIN, xpos, ypos, 0,
        base::Time::Now(), 0, 0, 1 << kTouchId);
    bool consumed = root_window->DispatchGestureEvent(event1);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(0, delegate->handle_percent_count());

    // No move at the beginning will produce no events.
    aura::GestureEvent* event2 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE,
        xpos, ypos, 0, base::Time::Now(), 0, 0, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event2);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(0, delegate->handle_percent_count());

    // A move to a new Y location will produce an event.
    aura::GestureEvent* event3 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE, xpos, ypos + ypos_half, 0,
        base::Time::Now(), 0, ypos_half, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event3);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(1, delegate->handle_percent_count());
    EXPECT_EQ(50.0, delegate->handle_percent());

    // A move to an illegal Y location will produce legal results.
    aura::GestureEvent* event4 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE, xpos, ypos - 100, 0,
        base::Time::Now(), 0, -ypos_half - 100, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event4);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(2, delegate->handle_percent_count());
    EXPECT_EQ(100.0, delegate->handle_percent());

    aura::GestureEvent* event5 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE, xpos, ypos + 2 * screen.height(), 0,
        base::Time::Now(), 0, 2 * screen.height() + 100, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event5);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(3, delegate->handle_percent_count());
    EXPECT_EQ(0.0, delegate->handle_percent());

    // Finishing the gesture should not change anything.
    aura::GestureEvent* event7 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_END, xpos, ypos + ypos_half, 0,
        base::Time::Now(), 0, 0, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event7);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(3, delegate->handle_percent_count());

    // Another event after this one should get ignored.
    aura::GestureEvent* event8 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE, xpos, ypos_half, 0,
        base::Time::Now(), 0, 0, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event8);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(3, delegate->handle_percent_count());

    aura::TouchEvent release1(ui::ET_TOUCH_RELEASED,
                              gfx::Point(2 * xpos, ypos + ypos_half), kTouchId,
                              base::Time::NowFromSystemTime() - base::Time());
    root_window->DispatchTouchEvent(&release1);
  }
}

// Ensure that the application control operations gets properly handled.
TEST_F(SystemGestureEventFilterTest, ApplicationControl) {
  aura::RootWindow* root_window = Shell::GetPrimaryRootWindow();

  gfx::Rect screen = gfx::Screen::GetPrimaryDisplay().bounds();
  int ypos_half = screen.height() / 2;

  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window0(
      aura::test::CreateTestWindowWithDelegate(
          &delegate, 9, gfx::Rect(0, 0, 100, 100), root_window));
  scoped_ptr<aura::Window> window1(
      aura::test::CreateTestWindowWithDelegate(
          &delegate, 10, gfx::Rect(0, 0, 100, 100), window0.get()));
  scoped_ptr<aura::Window> window2(
      aura::test::CreateTestWindowWithDelegate(
          &delegate, 11, gfx::Rect(0, 0, 100, 100), window0.get()));

  const int kTouchId = 5;

  for (int pass = 0; pass < 2; pass++) {
    // Add the launcher items and make sure the first item is the active one.
    TestLauncherDelegate::instance()->AddLauncherItem(window1.get(),
                                                      ash::STATUS_ACTIVE);
    TestLauncherDelegate::instance()->AddLauncherItem(window2.get(),
                                                      ash::STATUS_RUNNING);
    ash::wm::ActivateWindow(window1.get());

    int xpos = screen.x() - 10;
    int delta_x = 100;
    int ypos = screen.y();
    if (pass) {
      xpos = screen.right() + 40;  // Make sure the touch is out of the screen.
      delta_x = -100;
    }

    aura::Window* active_window = ash::wm::GetActiveWindow();

    // Get a target for kTouchId
    aura::TouchEvent press(ui::ET_TOUCH_PRESSED,
                           gfx::Point(-10, ypos + ypos_half),
                           kTouchId,
                           base::Time::NowFromSystemTime() - base::Time());
    root_window->DispatchTouchEvent(&press);

    aura::GestureEvent* event1 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_BEGIN, xpos, ypos, 0,
        base::Time::Now(), 0, 0, 1 << kTouchId);
    bool consumed = root_window->DispatchGestureEvent(event1);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(ash::wm::GetActiveWindow(), active_window);

    // No move at the beginning will produce no events.
    aura::GestureEvent* event2 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE,
        xpos, ypos, 0, base::Time::Now(), 0, 0, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event2);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(ash::wm::GetActiveWindow(), active_window);

    // A move further to the outside will not trigger an action.
    aura::GestureEvent* event3 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE, xpos - delta_x, ypos, 0,
        base::Time::Now(), -delta_x, 0, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event3);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(ash::wm::GetActiveWindow(), active_window);

    // A move to the proper side will trigger an action.
    aura::GestureEvent* event4 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE, xpos + delta_x, ypos, 0,
        base::Time::Now(), 2 * delta_x, 0, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event4);

    EXPECT_TRUE(consumed);
    EXPECT_NE(ash::wm::GetActiveWindow(), active_window);
    active_window = ash::wm::GetActiveWindow();

    // A second move to the proper side will not trigger an action.
    aura::GestureEvent* event5 = new aura::GestureEvent(
        ui::ET_GESTURE_SCROLL_UPDATE, xpos + 2 * delta_x, ypos, 0,
        base::Time::Now(), delta_x, 0, 1 << kTouchId);
    consumed = root_window->DispatchGestureEvent(event5);

    EXPECT_TRUE(consumed);
    EXPECT_EQ(ash::wm::GetActiveWindow(), active_window);

    aura::TouchEvent release(ui::ET_TOUCH_RELEASED,
                             gfx::Point(2 * xpos, ypos + ypos_half), kTouchId,
                             base::Time::NowFromSystemTime() - base::Time());
    root_window->DispatchTouchEvent(&release);

    // Remove the launcher items again.
    TestLauncherDelegate::instance()->OnWillRemoveWindow(window1.get());
    TestLauncherDelegate::instance()->OnWillRemoveWindow(window2.get());
  }
}

}  // namespace test
}  // namespace ash
