// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "chrome/browser/gtk/reload_button_gtk.h"
#include "testing/gtest/include/gtest/gtest.h"

class ReloadButtonGtkPeer {
 public:
  explicit ReloadButtonGtkPeer(ReloadButtonGtk* reload) : reload_(reload) { }

  // const accessors for internal state
  ReloadButtonGtk::Mode intended_mode() const {
    return reload_->intended_mode_;
  }
  ReloadButtonGtk::Mode visible_mode() const { return reload_->visible_mode_; }

  // mutators for internal state
  void SetState(GtkStateType state) {
    gtk_widget_set_state(reload_->widget(), state);
  }
  void set_intended_mode(ReloadButtonGtk::Mode mode) {
    reload_->intended_mode_ = mode;
  }
  void set_visible_mode(ReloadButtonGtk::Mode mode) {
    reload_->visible_mode_ = mode;
  }
  void set_timer_running() {
    reload_->pretend_timer_is_running_for_unittest_ = true;
  }
  void set_timer_stopped() {
    reload_->pretend_timer_is_running_for_unittest_ = false;
  }

  // forwarders to private methods
  gboolean OnLeave() {
    return reload_->OnLeaveNotify(reload_->widget(), NULL);
  }

  void OnClicked() {
    reload_->OnClicked(reload_->widget());
  }

 private:
  ReloadButtonGtk* const reload_;
};

namespace {

class ReloadButtonGtkTest : public testing::Test {
 protected:
  ReloadButtonGtkTest() : reload_(NULL, NULL), peer_(&reload_) { }

 private:
  MessageLoop message_loop_;

 protected:
  ReloadButtonGtk reload_;
  ReloadButtonGtkPeer peer_;
};

TEST_F(ReloadButtonGtkTest, ChangeModeReload) {
  reload_.ChangeMode(ReloadButtonGtk::MODE_RELOAD, true);
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.intended_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.visible_mode());
}

TEST_F(ReloadButtonGtkTest, ChangeModeStop) {
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, true);
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.intended_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.visible_mode());
}

TEST_F(ReloadButtonGtkTest, ScheduleChangeModeNormalReload) {
  peer_.set_visible_mode(ReloadButtonGtk::MODE_STOP);
  peer_.SetState(GTK_STATE_NORMAL);
  reload_.ChangeMode(ReloadButtonGtk::MODE_RELOAD, false);
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.intended_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.visible_mode());
}

TEST_F(ReloadButtonGtkTest, ScheduleChangeModeHotReload) {
  peer_.set_visible_mode(ReloadButtonGtk::MODE_STOP);
  peer_.SetState(GTK_STATE_PRELIGHT);
  reload_.ChangeMode(ReloadButtonGtk::MODE_RELOAD, false);
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.intended_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.visible_mode());
}

TEST_F(ReloadButtonGtkTest, ScheduleChangeModeNormalStop) {
  peer_.set_visible_mode(ReloadButtonGtk::MODE_RELOAD);
  peer_.SetState(GTK_STATE_NORMAL);
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.intended_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.visible_mode());
}

TEST_F(ReloadButtonGtkTest, ScheduleChangeModeHotStop) {
  peer_.set_visible_mode(ReloadButtonGtk::MODE_RELOAD);
  peer_.SetState(GTK_STATE_PRELIGHT);
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.intended_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.visible_mode());
}

TEST_F(ReloadButtonGtkTest, ScheduleChangeModeTimerHotStop) {
  peer_.set_visible_mode(ReloadButtonGtk::MODE_RELOAD);
  peer_.SetState(GTK_STATE_PRELIGHT);
  peer_.set_timer_running();
  reload_.ChangeMode(ReloadButtonGtk::MODE_STOP, false);
  peer_.set_timer_stopped();
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.intended_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.visible_mode());
}

TEST_F(ReloadButtonGtkTest, OnLeaveIntendedStop) {
  peer_.SetState(GTK_STATE_PRELIGHT);
  peer_.set_visible_mode(ReloadButtonGtk::MODE_RELOAD);
  peer_.set_intended_mode(ReloadButtonGtk::MODE_STOP);
  peer_.OnLeave();
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.visible_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_STOP, peer_.intended_mode());
}

TEST_F(ReloadButtonGtkTest, OnLeaveIntendedReload) {
  peer_.SetState(GTK_STATE_PRELIGHT);
  peer_.set_visible_mode(ReloadButtonGtk::MODE_STOP);
  peer_.set_intended_mode(ReloadButtonGtk::MODE_RELOAD);
  peer_.OnLeave();
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.visible_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.intended_mode());
}

TEST_F(ReloadButtonGtkTest, OnClickedStop) {
  peer_.set_visible_mode(ReloadButtonGtk::MODE_STOP);
  peer_.OnClicked();
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.visible_mode());
  EXPECT_EQ(ReloadButtonGtk::MODE_RELOAD, peer_.intended_mode());
}

}  // namespace
