// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/window_util.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace test {

namespace {

class DummyScreenshotDelegate : public ScreenshotDelegate {
 public:
  DummyScreenshotDelegate() : handle_take_screenshot_count_(0) {
  }
  virtual ~DummyScreenshotDelegate() {}

  // Overridden from ScreenshotDelegate:
  virtual void HandleTakeScreenshot(aura::Window* window) OVERRIDE {
    if (window != NULL)
      ++handle_take_screenshot_count_;
  }

  virtual void HandleTakePartialScreenshot(
      aura::Window* window, const gfx::Rect& rect) OVERRIDE {
    // Do nothing because it's not tested yet.
  }

  int handle_take_screenshot_count() const {
    return handle_take_screenshot_count_;
  }

 private:
  int handle_take_screenshot_count_;

  DISALLOW_COPY_AND_ASSIGN(DummyScreenshotDelegate);
};

AcceleratorController* GetController() {
  return Shell::GetInstance()->accelerator_controller();
}

}  // namespace

typedef AshTestBase AcceleratorFilterTest;

// Tests if AcceleratorFilter works without a focused window.
TEST_F(AcceleratorFilterTest, TestFilterWithoutFocus) {
  DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
  GetController()->SetScreenshotDelegate(
      scoped_ptr<ScreenshotDelegate>(delegate).Pass());
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  aura::test::EventGenerator generator(Shell::GetRootWindow());
  // AcceleratorController calls ScreenshotDelegate::HandleTakeScreenshot() when
  // VKEY_PRINT is pressed. See kAcceleratorData[] in accelerator_controller.cc.
  generator.PressKey(ui::VKEY_PRINT, 0);
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());
  generator.ReleaseKey(ui::VKEY_PRINT, 0);
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());
}

// Tests if AcceleratorFilter works as expected with a focused window.
TEST_F(AcceleratorFilterTest, TestFilterWithFocus) {
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer);
  aura::test::TestWindowDelegate test_delegate;
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
      &test_delegate,
      -1,
      gfx::Rect(),
      default_container));
  wm::ActivateWindow(window.get());

  DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
  GetController()->SetScreenshotDelegate(
      scoped_ptr<ScreenshotDelegate>(delegate).Pass());
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // AcceleratorFilter should ignore the key events since the root window is
  // not focused.
  aura::test::EventGenerator generator(Shell::GetRootWindow());
  generator.PressKey(ui::VKEY_PRINT, 0);
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());
  generator.ReleaseKey(ui::VKEY_PRINT, 0);
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  // Reset window before |test_delegate| gets deleted.
  window.reset();
}

// Tests if AcceleratorFilter ignores the flag for Caps Lock.
TEST_F(AcceleratorFilterTest, TestCapsLockMask) {
  DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
  GetController()->SetScreenshotDelegate(
      scoped_ptr<ScreenshotDelegate>(delegate).Pass());
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  aura::test::EventGenerator generator(Shell::GetRootWindow());
  generator.PressKey(ui::VKEY_PRINT, 0);
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());
  generator.ReleaseKey(ui::VKEY_PRINT, 0);
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());

  // Check if AcceleratorFilter ignores the mask for Caps Lock. Note that there
  // is no ui::EF_ mask for Num Lock.
  generator.PressKey(ui::VKEY_PRINT, ui::EF_CAPS_LOCK_DOWN);
  EXPECT_EQ(2, delegate->handle_take_screenshot_count());
  generator.ReleaseKey(ui::VKEY_PRINT, ui::EF_CAPS_LOCK_DOWN);
  EXPECT_EQ(2, delegate->handle_take_screenshot_count());
}

}  // namespace test
}  // namespace ash
