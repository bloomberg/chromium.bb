// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/priority_write_scheduler.h"

#include "net/test/gtest_util.h"

namespace net {
namespace test {
namespace {

class PriorityWriteSchedulerTest : public ::testing::Test {
 public:
  PriorityWriteScheduler<int> scheduler_;
};

TEST_F(PriorityWriteSchedulerTest, RegisterUnregisterStreams) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  scheduler_.RegisterStream(1, 1);

  // Try redundant registrations.
  EXPECT_DFATAL(scheduler_.RegisterStream(1, 1), "Stream 1 already registered");
  EXPECT_DFATAL(scheduler_.RegisterStream(1, 2), "Stream 1 already registered");

  scheduler_.RegisterStream(2, 3);

  // Verify registration != ready.
  EXPECT_FALSE(scheduler_.HasReadyStreams());

  scheduler_.UnregisterStream(1);
  scheduler_.UnregisterStream(2);

  // Try redundant unregistration.
  EXPECT_DFATAL(scheduler_.UnregisterStream(1), "Stream 1 not registered");
  EXPECT_DFATAL(scheduler_.UnregisterStream(2), "Stream 2 not registered");
}

TEST_F(PriorityWriteSchedulerTest, GetStreamPriority) {
  EXPECT_DFATAL(EXPECT_EQ(kV3LowestPriority, scheduler_.GetStreamPriority(1)),
                "Stream 1 not registered");

  scheduler_.RegisterStream(1, 3);
  EXPECT_EQ(3, scheduler_.GetStreamPriority(1));

  // Redundant registration shouldn't change stream priority.
  EXPECT_DFATAL(scheduler_.RegisterStream(1, 4), "Stream 1 already registered");
  EXPECT_EQ(3, scheduler_.GetStreamPriority(1));

  scheduler_.UpdateStreamPriority(1, 5);
  EXPECT_EQ(5, scheduler_.GetStreamPriority(1));

  // Toggling ready state shouldn't change stream priority.
  scheduler_.MarkStreamReady(1, true);
  EXPECT_EQ(5, scheduler_.GetStreamPriority(1));

  // Test changing priority of ready stream.
  EXPECT_EQ(1u, scheduler_.NumReadyStreams(5));
  scheduler_.UpdateStreamPriority(1, 6);
  EXPECT_EQ(6, scheduler_.GetStreamPriority(1));
  EXPECT_EQ(0u, scheduler_.NumReadyStreams(5));
  EXPECT_EQ(1u, scheduler_.NumReadyStreams(6));

  EXPECT_EQ(1, scheduler_.PopNextReadyStream());
  EXPECT_EQ(6, scheduler_.GetStreamPriority(1));

  scheduler_.UnregisterStream(1);
  EXPECT_DFATAL(EXPECT_EQ(kV3LowestPriority, scheduler_.GetStreamPriority(1)),
                "Stream 1 not registered");
}

TEST_F(PriorityWriteSchedulerTest, UpdateStreamPriority) {
  // Updating priority of unregistered stream should have no effect.
  EXPECT_DFATAL(EXPECT_EQ(kV3LowestPriority, scheduler_.GetStreamPriority(3)),
                "Stream 3 not registered");
  EXPECT_DFATAL(scheduler_.UpdateStreamPriority(3, 1),
                "Stream 3 not registered");
  EXPECT_DFATAL(EXPECT_EQ(kV3LowestPriority, scheduler_.GetStreamPriority(3)),
                "Stream 3 not registered");

  scheduler_.RegisterStream(3, 1);
  EXPECT_EQ(1, scheduler_.GetStreamPriority(3));
  scheduler_.UpdateStreamPriority(3, 2);
  EXPECT_EQ(2, scheduler_.GetStreamPriority(3));

  // Updating priority of stream to current priority value is valid, but has no
  // effect.
  scheduler_.UpdateStreamPriority(3, 2);
  EXPECT_EQ(2, scheduler_.GetStreamPriority(3));

  // Even though stream 4 is marked ready after stream 5, it should be returned
  // first by PopNextReadyStream() since it has higher priority.
  scheduler_.RegisterStream(4, 1);
  scheduler_.MarkStreamReady(3, false);  // priority 2
  scheduler_.MarkStreamReady(4, false);  // priority 1
  EXPECT_EQ(4, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3, scheduler_.PopNextReadyStream());

  // Verify that lowering priority of stream 4 causes it to be returned later
  // by PopNextReadyStream().
  scheduler_.MarkStreamReady(3, false);  // priority 2
  scheduler_.MarkStreamReady(4, false);  // priority 1
  scheduler_.UpdateStreamPriority(4, 3);
  EXPECT_EQ(3, scheduler_.PopNextReadyStream());
  EXPECT_EQ(4, scheduler_.PopNextReadyStream());

  scheduler_.UnregisterStream(3);
  EXPECT_DFATAL(scheduler_.UpdateStreamPriority(3, 1),
                "Stream 3 not registered");
}

TEST_F(PriorityWriteSchedulerTest, HasHigherPriorityReadyStream) {
  EXPECT_DFATAL(EXPECT_FALSE(scheduler_.HasHigherPriorityReadyStream(1)),
                "Stream 1 not registered");

  // Add ready streams of lower and equal priority.
  scheduler_.RegisterStream(1, 4);
  EXPECT_FALSE(scheduler_.HasHigherPriorityReadyStream(1));
  scheduler_.RegisterStream(2, 5);
  scheduler_.MarkStreamReady(2, false);
  EXPECT_FALSE(scheduler_.HasHigherPriorityReadyStream(1));
  scheduler_.RegisterStream(3, 4);
  scheduler_.MarkStreamReady(3, false);
  EXPECT_FALSE(scheduler_.HasHigherPriorityReadyStream(1));

  // Verify that registration of a stream with higher priority isn't
  // sufficient--it needs to be marked ready.
  scheduler_.RegisterStream(4, 3);
  EXPECT_FALSE(scheduler_.HasHigherPriorityReadyStream(1));
  scheduler_.MarkStreamReady(4, false);
  EXPECT_TRUE(scheduler_.HasHigherPriorityReadyStream(1));

  // Verify method is responsive to changes in priority.
  scheduler_.UpdateStreamPriority(1, 2);
  EXPECT_FALSE(scheduler_.HasHigherPriorityReadyStream(1));
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamReadyBack) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_DFATAL(scheduler_.MarkStreamReady(1, false),
                "Stream 1 not registered");
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_DFATAL(EXPECT_EQ(0, scheduler_.PopNextReadyStream()),
                "No ready streams available");

  // Add a bunch of ready streams to tail of per-priority lists.
  // Expected order: (P2) 4, (P3) 1, 2, 3, (P5) 5.
  scheduler_.RegisterStream(1, 3);
  scheduler_.MarkStreamReady(1, false);
  EXPECT_TRUE(scheduler_.HasReadyStreams());
  scheduler_.RegisterStream(2, 3);
  scheduler_.MarkStreamReady(2, false);
  scheduler_.RegisterStream(3, 3);
  scheduler_.MarkStreamReady(3, false);
  scheduler_.RegisterStream(4, 2);
  scheduler_.MarkStreamReady(4, false);
  scheduler_.RegisterStream(5, 5);
  scheduler_.MarkStreamReady(5, false);

  EXPECT_EQ(4, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5, scheduler_.PopNextReadyStream());
  EXPECT_DFATAL(EXPECT_EQ(0, scheduler_.PopNextReadyStream()),
                "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamReadyFront) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_DFATAL(scheduler_.MarkStreamReady(1, true), "Stream 1 not registered");
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_DFATAL(EXPECT_EQ(0, scheduler_.PopNextReadyStream()),
                "No ready streams available");

  // Add a bunch of ready streams to head of per-priority lists.
  // Expected order: (P2) 4, (P3) 3, 2, 1, (P5) 5
  scheduler_.RegisterStream(1, 3);
  scheduler_.MarkStreamReady(1, true);
  EXPECT_TRUE(scheduler_.HasReadyStreams());
  scheduler_.RegisterStream(2, 3);
  scheduler_.MarkStreamReady(2, true);
  scheduler_.RegisterStream(3, 3);
  scheduler_.MarkStreamReady(3, true);
  scheduler_.RegisterStream(4, 2);
  scheduler_.MarkStreamReady(4, true);
  scheduler_.RegisterStream(5, 5);
  scheduler_.MarkStreamReady(5, true);

  EXPECT_EQ(4, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5, scheduler_.PopNextReadyStream());
  EXPECT_DFATAL(EXPECT_EQ(0, scheduler_.PopNextReadyStream()),
                "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamReadyBackAndFront) {
  scheduler_.RegisterStream(1, 4);
  scheduler_.RegisterStream(2, 3);
  scheduler_.RegisterStream(3, 3);
  scheduler_.RegisterStream(4, 3);
  scheduler_.RegisterStream(5, 4);
  scheduler_.RegisterStream(6, 1);

  // Add a bunch of ready streams to per-priority lists, with variety of adding
  // at head and tail.
  // Expected order: (P1) 6, (P3) 4, 2, 3, (P4) 1, 5
  scheduler_.MarkStreamReady(1, true);
  scheduler_.MarkStreamReady(2, true);
  scheduler_.MarkStreamReady(3, false);
  scheduler_.MarkStreamReady(4, true);
  scheduler_.MarkStreamReady(5, false);
  scheduler_.MarkStreamReady(6, true);

  EXPECT_EQ(6, scheduler_.PopNextReadyStream());
  EXPECT_EQ(4, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5, scheduler_.PopNextReadyStream());
  EXPECT_DFATAL(EXPECT_EQ(0, scheduler_.PopNextReadyStream()),
                "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamNotReady) {
  // Verify ready state reflected in NumReadyStreams().
  scheduler_.RegisterStream(1, 1);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());
  scheduler_.MarkStreamReady(1, false);
  EXPECT_EQ(1u, scheduler_.NumReadyStreams());
  scheduler_.MarkStreamNotReady(1);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());

  // Empty pop should fail.
  EXPECT_DFATAL(EXPECT_EQ(0, scheduler_.PopNextReadyStream()),
                "No ready streams available");

  // Tolerate redundant marking of a stream as not ready.
  scheduler_.MarkStreamNotReady(1);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());

  // Should only be able to mark registered streams.
  EXPECT_DFATAL(scheduler_.MarkStreamNotReady(3), "Stream 3 not registered");
}

TEST_F(PriorityWriteSchedulerTest, UnregisterRemovesStream) {
  scheduler_.RegisterStream(3, 4);
  scheduler_.MarkStreamReady(3, false);
  EXPECT_EQ(1u, scheduler_.NumReadyStreams());

  // Unregistering a stream should remove it from set of ready streams.
  scheduler_.UnregisterStream(3);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());
  EXPECT_DFATAL(EXPECT_EQ(0, scheduler_.PopNextReadyStream()),
                "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, ShouldYield) {
  scheduler_.RegisterStream(1, 1);
  scheduler_.RegisterStream(4, 4);
  scheduler_.RegisterStream(5, 4);
  scheduler_.RegisterStream(7, 7);

  // Make sure we don't yield when the list is empty.
  EXPECT_FALSE(scheduler_.ShouldYield(1));

  // Add a low priority stream.
  scheduler_.MarkStreamReady(4, false);
  // 4 should not yield to itself.
  EXPECT_FALSE(scheduler_.ShouldYield(4));
  // 7 should yield as 4 is blocked and a higher priority.
  EXPECT_TRUE(scheduler_.ShouldYield(7));
  // 5 should yield to 4 as they are the same priority.
  EXPECT_TRUE(scheduler_.ShouldYield(5));
  // 1 should not yield as 1 is higher priority.
  EXPECT_FALSE(scheduler_.ShouldYield(1));

  // Add a second stream in that priority class.
  scheduler_.MarkStreamReady(5, false);
  // 4 and 5 are both blocked, but 4 is at the front so should not yield.
  EXPECT_FALSE(scheduler_.ShouldYield(4));
  EXPECT_TRUE(scheduler_.ShouldYield(5));
}

}  // namespace
}  // namespace test
}  // namespace net
