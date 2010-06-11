// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/util/channel.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestEvent {
  explicit TestEvent(int foo) : data(foo) {}
  int data;
};

class TestObserver : public browser_sync::ChannelEventHandler<TestEvent> {
 public:
  virtual void HandleChannelEvent(const TestEvent& event) {
    delete hookup;
    hookup = 0;
  }

  browser_sync::ChannelHookup<TestEvent>* hookup;
};

TEST(ChannelTest, RemoveOnNotify) {
  browser_sync::Channel<TestEvent> channel;
  TestObserver observer;

  observer.hookup = channel.AddObserver(&observer);

  ASSERT_TRUE(0 != observer.hookup);
  channel.Notify(TestEvent(1));
  ASSERT_EQ(0, observer.hookup);
}
