// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/gtk/reload_button_gtk.h"
#include "testing/gtest/include/gtest/gtest.h"

class ReloadButtonGtkTest : public testing::Test {
 public:
  ReloadButtonGtkTest();

  void CheckState(bool enabled,
                  ReloadButtonGtk::Mode intended_mode,
                  ReloadButtonGtk::Mode visible_mode,
                  bool timer_running);

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
  // Set the timer delay to 0 so that timers will fire as soon as we tell the
  // message loop to run pending tasks.
  reload_.timer_delay_ = base::TimeDelta();
}

void ReloadButtonGtkTest::CheckState(bool enabled,
                                     ReloadButtonGtk::Mode intended_mode,
                                     ReloadButtonGtk::Mode visible_mode,
                                     bool timer_running) {
  EXPECT_NE(enabled, reload_.stop_.paint_override() == GTK_STATE_INSENSITIVE);
  EXPECT_EQ(intended_mode, reload_.intended_mode_);
  EXPECT_EQ(visible_mode, reload_.visible_mode_);
  EXPECT_EQ(timer_running, reload_.timer_.IsRunning());
}

TEST_F(ReloadButtonGtkTest, Basic) {
  // The stop/reload button starts in the "enabled reload" state with no timer
  // running.
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false);

  // Press the button.  This should start the double-click timer.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             true);

  // Now change the mode (as if the browser had started loading the page).  This
  // should cancel the timer since the button is not hovered.
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  CheckState(true, ReloadButtonGtk::MODE_STOP, ReloadButtonGtk::MODE_STOP,
             false);

  // Press the button again.  This should change back to reload.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false);
}

TEST_F(ReloadButtonGtkTest, DoubleClickTimer) {
  // Start by pressing the button.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));

  // Try to press the button again.  This should do nothing because the timer is
  // running.
  int original_reload_count = reload_count();
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             true);
  EXPECT_EQ(original_reload_count, reload_count());

  // Hover the button, and change mode.  The visible mode should not change,
  // again because the timer is running.
  set_mouse_hovered(true);
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  CheckState(true, ReloadButtonGtk::MODE_STOP, ReloadButtonGtk::MODE_RELOAD,
             true);

  // Now fire the timer.  This should complete the mode change.
  loop_.RunAllPending();
  CheckState(true, ReloadButtonGtk::MODE_STOP, ReloadButtonGtk::MODE_STOP,
             false);
}

TEST_F(ReloadButtonGtkTest, DisableOnHover) {
  // Start by pressing the button and proceeding with the mode change.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);

  // Now hover the button and change back.  This should result in a disabled
  // stop button.
  set_mouse_hovered(true);
  reload_.ChangeMode(ReloadButtonGtk::MODE_RELOAD, false);
  CheckState(false, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_STOP,
             false);

  // Un-hover the button, which should allow it to reset.
  set_mouse_hovered(false);
  fake_mouse_leave();
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false);
}

TEST_F(ReloadButtonGtkTest, ResetOnClick) {
  // Start by pressing the button and proceeding with the mode change.
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);

  // Hover the button and click it.  This should change back to reload despite
  // the hover, because it's a direct user action.
  set_mouse_hovered(true);
  gtk_button_clicked(GTK_BUTTON(reload_.widget()));
  CheckState(true, ReloadButtonGtk::MODE_RELOAD, ReloadButtonGtk::MODE_RELOAD,
             false);
}
