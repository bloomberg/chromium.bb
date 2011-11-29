// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point.h"

class TestMouseObserver : public PanelMouseWatcherObserver {
 public:
  TestMouseObserver() : mouse_movements_(0) {}
  // Overridden from PanelMouseWatcherObserver:
  virtual void OnMouseMove(const gfx::Point& mouse_position) OVERRIDE {
    ++mouse_movements_;
  }
  int mouse_movements_;
};

class PanelMouseWatcherTest : public testing::Test {
};

TEST_F(PanelMouseWatcherTest, StartStopWatching) {
  MessageLoop loop(MessageLoop::TYPE_UI);

  scoped_ptr<PanelMouseWatcher> watcher(PanelMouseWatcher::Create());
  EXPECT_FALSE(watcher->IsActive());

  scoped_ptr<TestMouseObserver> user1(new TestMouseObserver());
  scoped_ptr<TestMouseObserver> user2(new TestMouseObserver());

  // No observers.
  watcher->NotifyMouseMovement(gfx::Point(42, 101));
  EXPECT_EQ(0, user1->mouse_movements_);
  EXPECT_EQ(0, user2->mouse_movements_);

  // Only one mouse observer.
  watcher->AddObserver(user1.get());
  EXPECT_TRUE(watcher->IsActive());
  watcher->NotifyMouseMovement(gfx::Point(42, 101));
  EXPECT_GE(user1->mouse_movements_, 1);
  EXPECT_EQ(0, user2->mouse_movements_);
  watcher->RemoveObserver(user1.get());
  EXPECT_FALSE(watcher->IsActive());

  // More than one mouse observer.
  watcher->AddObserver(user1.get());
  EXPECT_TRUE(watcher->IsActive());
  watcher->AddObserver(user2.get());
  watcher->NotifyMouseMovement(gfx::Point(101, 42));
  EXPECT_GE(user1->mouse_movements_, 2);
  EXPECT_GE(user2->mouse_movements_, 1);

  // Back to one observer.
  watcher->RemoveObserver(user1.get());
  EXPECT_TRUE(watcher->IsActive());
  int saved_count = user1->mouse_movements_;
  watcher->NotifyMouseMovement(gfx::Point(1, 2));
  EXPECT_EQ(saved_count, user1->mouse_movements_);
  EXPECT_GE(user2->mouse_movements_, 2);
  watcher->RemoveObserver(user2.get());
  EXPECT_FALSE(watcher->IsActive());
}
