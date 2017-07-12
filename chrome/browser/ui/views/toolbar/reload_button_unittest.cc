// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button.h"

#include "base/run_loop.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/test_views_delegate.h"

class ReloadButtonTest : public ChromeRenderViewHostTestHarness {
 public:
  ReloadButtonTest();

  void CheckState(bool enabled,
                  ReloadButton::Mode intended_mode,
                  ReloadButton::Mode visible_mode,
                  bool double_click_timer_running,
                  bool mode_switch_timer_running);

  // These accessors eliminate the need to declare each testcase as a friend.
  void set_mouse_hovered(bool hovered) {
    reload_.testing_mouse_hovered_ = hovered;
  }
  int reload_count() { return reload_.testing_reload_count_; }

 protected:
  ReloadButton* reload() { return &reload_; }

 private:
  views::TestViewsDelegate views_delegate_;
  ReloadButton reload_;

  DISALLOW_COPY_AND_ASSIGN(ReloadButtonTest);
};

ReloadButtonTest::ReloadButtonTest() : reload_(profile(), nullptr) {
  // Set the timer delays to 0 so that timers will fire as soon as we tell the
  // message loop to run pending tasks.
  reload_.double_click_timer_delay_ = base::TimeDelta();
  reload_.mode_switch_timer_delay_ = base::TimeDelta();
}

void ReloadButtonTest::CheckState(bool enabled,
                                  ReloadButton::Mode intended_mode,
                                  ReloadButton::Mode visible_mode,
                                  bool double_click_timer_running,
                                  bool mode_switch_timer_running) {
  EXPECT_EQ(enabled, reload_.enabled());
  EXPECT_EQ(intended_mode, reload_.intended_mode_);
  EXPECT_EQ(visible_mode, reload_.visible_mode_);
  EXPECT_EQ(double_click_timer_running,
            reload_.double_click_timer_.IsRunning());
  EXPECT_EQ(mode_switch_timer_running, reload_.mode_switch_timer_.IsRunning());
}

TEST_F(ReloadButtonTest, Basic) {
  // The stop/reload button starts in the "enabled reload" state with no timers
  // running.
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);

  // Press the button.  This should start the double-click timer.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  reload()->ButtonPressed(reload(), e);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, true,
             false);

  // Now change the mode (as if the browser had started loading the page).  This
  // should cancel the double-click timer since the button is not hovered.
  reload()->ChangeMode(ReloadButton::MODE_STOP, false);
  CheckState(true, ReloadButton::MODE_STOP, ReloadButton::MODE_STOP, false,
             false);

  // Press the button again.  This should change back to reload.
  reload()->ButtonPressed(reload(), e);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);
}

TEST_F(ReloadButtonTest, DoubleClickTimer) {
  // Start by pressing the button.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  reload()->ButtonPressed(reload(), e);

  // Try to press the button again.  This should do nothing because the timer is
  // running.
  int original_reload_count = reload_count();
  reload()->ButtonPressed(reload(), e);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, true,
             false);
  EXPECT_EQ(original_reload_count, reload_count());

  // Hover the button, and change mode.  The visible mode should not change,
  // again because the timer is running.
  set_mouse_hovered(true);
  reload()->ChangeMode(ReloadButton::MODE_STOP, false);
  CheckState(true, ReloadButton::MODE_STOP, ReloadButton::MODE_RELOAD, true,
             false);

  // Now fire the timer.  This should complete the mode change.
  base::RunLoop().RunUntilIdle();
  CheckState(true, ReloadButton::MODE_STOP, ReloadButton::MODE_STOP, false,
             false);
}

TEST_F(ReloadButtonTest, DisableOnHover) {
  // Change to stop and hover.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  reload()->ButtonPressed(reload(), e);
  reload()->ChangeMode(ReloadButton::MODE_STOP, false);
  set_mouse_hovered(true);

  // Now change back to reload.  This should result in a disabled stop button
  // due to the hover.
  reload()->ChangeMode(ReloadButton::MODE_RELOAD, false);
  CheckState(false, ReloadButton::MODE_RELOAD, ReloadButton::MODE_STOP, false,
             true);

  // Un-hover the button, which should allow it to reset.
  set_mouse_hovered(false);
  ui::MouseEvent e2(ui::ET_MOUSE_MOVED, gfx::Point(), gfx::Point(),
                    ui::EventTimeForNow(), 0, 0);
  reload()->OnMouseExited(e2);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);
}

TEST_F(ReloadButtonTest, ResetOnClick) {
  // Change to stop and hover.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  reload()->ButtonPressed(reload(), e);
  reload()->ChangeMode(ReloadButton::MODE_STOP, false);
  set_mouse_hovered(true);

  // Press the button.  This should change back to reload despite the hover,
  // because it's a direct user action.
  reload()->ButtonPressed(reload(), e);
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);
}

TEST_F(ReloadButtonTest, ResetOnTimer) {
  // Change to stop, hover, and change back to reload.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  reload()->ButtonPressed(reload(), e);
  reload()->ChangeMode(ReloadButton::MODE_STOP, false);
  set_mouse_hovered(true);
  reload()->ChangeMode(ReloadButton::MODE_RELOAD, false);

  // Now fire the stop-to-reload timer.  This should reset the button.
  base::RunLoop().RunUntilIdle();
  CheckState(true, ReloadButton::MODE_RELOAD, ReloadButton::MODE_RELOAD, false,
             false);
}
