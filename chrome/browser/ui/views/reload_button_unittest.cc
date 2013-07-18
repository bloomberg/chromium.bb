// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/views/reload_button.h"
#include "testing/gtest/include/gtest/gtest.h"

class ReloadButtonTest : public testing::Test {
 public:
  ReloadButtonTest();

  void CheckState(bool enabled,
                  ReloadButton::Mode intended_mode,
                  ReloadButton::Mode visible_mode,
                  bool double_click_timer_running,
                  bool stop_to_reload_timer_running);

  // These accessors eliminate the need to declare each testcase as a friend.
  void set_mouse_hovered(bool hovered) {
    reload_.testing_mouse_hovered_ = hovered;
  }
  int reload_count() { return reload_.testing_reload_count_; }

 protected:
  // We need a message loop for the timers to post events.
  base::MessageLoop loop_;

  ReloadButton reload_;
};

ReloadButtonTest::ReloadButtonTest() : reload_(NULL, NULL) {
  // Set the timer delays to 0 so that timers will fire as soon as we tell the
  // message loop to run pending tasks.
  reload_.double_click_timer_delay_ = base::TimeDelta();
  reload_.stop_to_reload_timer_delay_ = base::TimeDelta();
}

void ReloadButtonTest::CheckState(bool enabled,
                                  ReloadButton::Mode intended_mode,
                                  ReloadButton::Mode visible_mode,
                                  bool double_click_timer_running,
                                  bool stop_to_reload_timer_running) {
  EXPECT_EQ(enabled, reload_.enabled());
  EXPECT_EQ(intended_mode, reload_.intended_mode_);
  EXPECT_EQ(visible_mode, reload_.visible_mode_);
  EXPECT_EQ(double_click_timer_running,
            reload_.double_click_timer_.IsRunning());
  EXPECT_EQ(stop_to_reload_timer_running,
            reload_.stop_to_reload_timer_.IsRunning());
}

TEST_F(ReloadButtonTest, Basic) {
  // The stop/reload button starts in the "enabled reload" state with no timers
  // running.
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);

  // Press the button.  This should start the double-click timer.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), 0);
  reload_.ButtonPressed(&reload_, e);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, true,
             false);

  // Now change the mode (as if the browser had started loading the page).  This
  // should cancel the double-click timer since the button is not hovered.
  reload_.ChangeMode(ReloadButton::MODE_STOP, false);
  CheckState(true, ReloadButton::MODE_STOP, ReloadButton::MODE_STOP, false,
             false);

  // Press the button again.  This should change back to reload.
  reload_.ButtonPressed(&reload_, e);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);
}

TEST_F(ReloadButtonTest, DoubleClickTimer) {
  // Start by pressing the button.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), 0);
  reload_.ButtonPressed(&reload_, e);

  // Try to press the button again.  This should do nothing because the timer is
  // running.
  int original_reload_count = reload_count();
  reload_.ButtonPressed(&reload_, e);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, true,
             false);
  EXPECT_EQ(original_reload_count, reload_count());

  // Hover the button, and change mode.  The visible mode should not change,
  // again because the timer is running.
  set_mouse_hovered(true);
  reload_.ChangeMode(ReloadButton::MODE_STOP, false);
  CheckState(true, ReloadButton::MODE_STOP, ReloadButton::MODE_RELOAD, true,
             false);

  // Now fire the timer.  This should complete the mode change.
  loop_.RunUntilIdle();
  CheckState(true, ReloadButton::MODE_STOP, ReloadButton::MODE_STOP, false,
             false);
}

TEST_F(ReloadButtonTest, DisableOnHover) {
  // Change to stop and hover.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), 0);
  reload_.ButtonPressed(&reload_, e);
  reload_.ChangeMode(ReloadButton::MODE_STOP, false);
  set_mouse_hovered(true);

  // Now change back to reload.  This should result in a disabled stop button
  // due to the hover.
  reload_.ChangeMode(ReloadButton::MODE_RELOAD, false);
  CheckState(false, ReloadButton::MODE_RELOAD, ReloadButton::MODE_STOP, false,
             true);

  // Un-hover the button, which should allow it to reset.
  set_mouse_hovered(false);
  ui::MouseEvent e2(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(), 0);
  reload_.OnMouseExited(e2);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);
}

TEST_F(ReloadButtonTest, ResetOnClick) {
  // Change to stop and hover.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), 0);
  reload_.ButtonPressed(&reload_, e);
  reload_.ChangeMode(ReloadButton::MODE_STOP, false);
  set_mouse_hovered(true);

  // Press the button.  This should change back to reload despite the hover,
  // because it's a direct user action.
  reload_.ButtonPressed(&reload_, e);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);
}

TEST_F(ReloadButtonTest, ResetOnTimer) {
  // Change to stop, hover, and change back to reload.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), 0);
  reload_.ButtonPressed(&reload_, e);
  reload_.ChangeMode(ReloadButton::MODE_STOP, false);
  set_mouse_hovered(true);
  reload_.ChangeMode(ReloadButton::MODE_RELOAD, false);

  // Now fire the stop-to-reload timer.  This should reset the button.
  loop_.RunUntilIdle();
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);
}
