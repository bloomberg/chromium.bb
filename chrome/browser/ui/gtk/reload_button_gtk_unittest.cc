// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/ui/gtk/reload_button_gtk.h"
#include "testing/gtest/include/gtest/gtest.h"

class ReloadButtonGtkTest : public testing::Test {
 public:
  ReloadButtonGtkTest();

  void CheckState(bool enabled,
                  ReloadButtonGtk::Mode intended_mode,
                  ReloadButtonGtk::Mode visible_mode,
                  bool double_click_timer_running,
                  bool stop_to_reload_timer_running);

  // These accessors eliminate the need to declare each testcase as a friend.
  void set_mouse_hovered(bool hovered) {
    reload_.testing_mouse_hovered_ = hovered;
  }
  int reload_count() { return reload_.testing_reload_count_; }
  void fake_mouse_leave() { reload_.OnLeaveNotify(reload_.widget(), NULL); }

 protected:
  // We need a message loop for the timers to post events.
  MessageLoop loop_;

  ReloadButtonGtk reload_;
};

ReloadButtonGtkTest::ReloadButtonGtkTest() : reload_(NULL, NULL) {
  // Set the timer delays to 0 so that timers will fire as soon as we tell the
  // message loop to run pending tasks.
  reload_.double_click_timer_delay_ = base::TimeDelta();
  reload_.stop_to_reload_timer_delay_ = base::TimeDelta();
}

void ReloadButtonGtkTest::CheckState(bool enabled,
                                     ReloadButtonGtk::Mode intended_mode,
                                     ReloadButtonGtk::Mode visible_mode,
                                     bool double_click_timer_running,
                                     bool stop_to_reload_timer_running) {
  EXPECT_NE(enabled, reload_.stop_.paint_override() == GTK_STATE_INSENSITIVE);
  EXPECT_EQ(intended_mode, reload_.intended_mode_);
  EXPECT_EQ(visible_mode, reload_.visible_mode_);
  EXPECT_EQ(double_click_timer_running,
            reload_.double_click_timer_.IsRunning());
  EXPECT_EQ(stop_to_reload_timer_running,
            reload_.stop_to_reload_timer_.IsRunning());
}

TEST_F(ReloadButtonGtkTest, Basic) {
  // The stop/reload button starts in the "enabled reload" state with no timers
  // running.
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false, false);

  // Press the button.  This should start the double-click timer.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             true, false);

  // Now change the mode (as if the browser had started loading the page).  This
  // should cancel the double-click timer since the button is not hovered.
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  CheckState(true, ReloadButtonGtk::MODE_STOP, ReloadButtonGtk::MODE_STOP,
             false, false);

  // Press the button again.  This should change back to reload.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false, false);
}

TEST_F(ReloadButtonGtkTest, DoubleClickTimer) {
  // Start by pressing the button.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));

  // Try to press the button again.  This should do nothing because the timer is
  // running.
  int original_reload_count = reload_count();
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             true, false);
  EXPECT_EQ(original_reload_count, reload_count());

  // Hover the button, and change mode.  The visible mode should not change,
  // again because the timer is running.
  set_mouse_hovered(true);
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  CheckState(true, ReloadButtonGtk::MODE_STOP, ReloadButtonGtk::MODE_RELOAD,
             true, false);

  // Now fire the timer.  This should complete the mode change.
  loop_.RunAllPending();
  CheckState(true, ReloadButtonGtk::MODE_STOP, ReloadButtonGtk::MODE_STOP,
             false, false);
}

TEST_F(ReloadButtonGtkTest, DisableOnHover) {
  // Change to stop and hover.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  set_mouse_hovered(true);

  // Now change back to reload.  This should result in a disabled stop button
  // due to the hover.
  reload_.ChangeMode(ReloadButtonGtk::MODE_RELOAD, false);
  CheckState(false, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_STOP,
             false, true);

  // Un-hover the button, which should allow it to reset.
  set_mouse_hovered(false);
  fake_mouse_leave();
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false, false);
}

TEST_F(ReloadButtonGtkTest, ResetOnClick) {
  // Change to stop and hover.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  set_mouse_hovered(true);

  // Press the button.  This should change back to reload despite the hover,
  // because it's a direct user action.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false, false);
}

TEST_F(ReloadButtonGtkTest, ResetOnTimer) {
  // Change to stop, hover, and change back to reload.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  set_mouse_hovered(true);
  reload_.ChangeMode(ReloadButtonGtk::MODE_RELOAD, false);

  // Now fire the stop-to-reload timer.  This should reset the button.
  loop_.RunAllPending();
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false, false);
}
