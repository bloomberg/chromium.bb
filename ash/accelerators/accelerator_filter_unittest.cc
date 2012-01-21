// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_filter.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/aura_shell_test_base.h"
#include "ash/wm/root_window_event_filter.h"
#include "ash/wm/window_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
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

AcceleratorController* GetController() {
  return Shell::GetInstance()->accelerator_controller();
}

}  // namespace

typedef AuraShellTestBase AcceleratorFilterTest;

// Tests if AcceleratorFilter ignores the flag for Caps Lock.
TEST_F(AcceleratorFilterTest, TestCapsLockMask) {
  // We need an active window. Otherwise, the root window will not forward a key
  // event to event filters.
  aura::Window* default_container = Shell::GetInstance()->GetContainer(
      internal::kShellWindowId_DefaultContainer);
  aura::Window* window = aura::test::CreateTestWindowWithDelegate(
      new aura::test::TestWindowDelegate,
      -1,
      gfx::Rect(),
      default_container);
  ActivateWindow(window);

  DummyScreenshotDelegate* delegate = new DummyScreenshotDelegate;
  GetController()->SetScreenshotDelegate(
      scoped_ptr<ScreenshotDelegate>(delegate).Pass());
  EXPECT_EQ(0, delegate->handle_take_screenshot_count());

  aura::test::EventGenerator generator_;
  // AcceleratorController calls ScreenshotDelegate::HandleTakeScreenshot() when
  // VKEY_PRINT is pressed. See kAcceleratorData[] in accelerator_controller.cc.
  generator_.PressKey(ui::VKEY_PRINT, 0);
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());
  generator_.ReleaseKey(ui::VKEY_PRINT, 0);
  EXPECT_EQ(1, delegate->handle_take_screenshot_count());

  // Check if AcceleratorFilter ignores the mask for Caps Lock. Note that there
  // is no ui::EF_ mask for Num Lock.
  generator_.PressKey(ui::VKEY_PRINT, ui::EF_CAPS_LOCK_DOWN);
  EXPECT_EQ(2, delegate->handle_take_screenshot_count());
  generator_.ReleaseKey(ui::VKEY_PRINT, ui::EF_CAPS_LOCK_DOWN);
  EXPECT_EQ(2, delegate->handle_take_screenshot_count());
}

}  // namespace test
}  // namespace ash
