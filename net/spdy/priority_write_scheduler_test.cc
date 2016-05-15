// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/priority_write_scheduler.h"

#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_test_utils.h"
#include "net/test/gtest_util.h"

namespace net {
namespace test {

template <typename StreamIdType>
class PriorityWriteSchedulerPeer {
 public:
  explicit PriorityWriteSchedulerPeer(
      PriorityWriteScheduler<StreamIdType>* scheduler)
      : scheduler_(scheduler) {}

  size_t NumReadyStreams(SpdyPriority priority) const {
    return scheduler_->priority_infos_[priority].ready_list.size();
  }

 private:
  PriorityWriteScheduler<StreamIdType>* scheduler_;
};

namespace {

class PriorityWriteSchedulerTest : public ::testing::Test {
 public:
  PriorityWriteSchedulerTest() : peer_(&scheduler_) {}

  PriorityWriteScheduler<unsigned int> scheduler_;
  PriorityWriteSchedulerPeer<unsigned int> peer_;
};

TEST_F(PriorityWriteSchedulerTest, RegisterUnregisterStreams) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_FALSE(scheduler_.StreamRegistered(1));
  scheduler_.RegisterStream(1, 1);
  EXPECT_TRUE(scheduler_.StreamRegistered(1));

  // Root stream counts as already registered.
  EXPECT_SPDY_BUG(scheduler_.RegisterStream(kHttp2RootStreamId, 1),
                  "Stream 0 already registered");

  // Try redundant registrations.
  EXPECT_SPDY_BUG(scheduler_.RegisterStream(1, 1),
                  "Stream 1 already registered");
  EXPECT_SPDY_BUG(scheduler_.RegisterStream(1, 2),
                  "Stream 1 already registered");

  scheduler_.RegisterStream(2, 3);

  // Verify registration != ready.
  EXPECT_FALSE(scheduler_.HasReadyStreams());

  scheduler_.UnregisterStream(1);
  scheduler_.UnregisterStream(2);

  // Try redundant unregistration.
  EXPECT_SPDY_BUG(scheduler_.UnregisterStream(1), "Stream 1 not registered");
  EXPECT_SPDY_BUG(scheduler_.UnregisterStream(2), "Stream 2 not registered");
}

TEST_F(PriorityWriteSchedulerTest, RegisterStream) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_FALSE(scheduler_.StreamRegistered(1));
  scheduler_.RegisterStream(1, kHttp2RootStreamId, 123, false);
  EXPECT_TRUE(scheduler_.StreamRegistered(1));
  EXPECT_EQ(Http2WeightToSpdyPriority(123), scheduler_.GetStreamPriority(1));
  EXPECT_EQ(kHttp2RootStreamId, scheduler_.GetStreamParent(1));
  EXPECT_FALSE(scheduler_.HasReadyStreams());

  EXPECT_SPDY_BUG(scheduler_.RegisterStream(1, kHttp2RootStreamId, 456, false),
                  "Stream 1 already registered");
  EXPECT_EQ(Http2WeightToSpdyPriority(123), scheduler_.GetStreamPriority(1));

  EXPECT_SPDY_BUG(scheduler_.RegisterStream(2, 3, 123, false),
                  "Stream 3 not registered");
  EXPECT_TRUE(scheduler_.StreamRegistered(2));
}

TEST_F(PriorityWriteSchedulerTest, GetStreamPriority) {
  EXPECT_SPDY_BUG(EXPECT_EQ(kV3LowestPriority, scheduler_.GetStreamPriority(1)),
                  "Stream 1 not registered");

  scheduler_.RegisterStream(1, 3);
  EXPECT_EQ(3, scheduler_.GetStreamPriority(1));

  // Redundant registration shouldn't change stream priority.
  EXPECT_SPDY_BUG(scheduler_.RegisterStream(1, 4),
                  "Stream 1 already registered");
  EXPECT_EQ(3, scheduler_.GetStreamPriority(1));

  scheduler_.UpdateStreamPriority(1, 5);
  EXPECT_EQ(5, scheduler_.GetStreamPriority(1));

  // Toggling ready state shouldn't change stream priority.
  scheduler_.MarkStreamReady(1, true);
  EXPECT_EQ(5, scheduler_.GetStreamPriority(1));

  // Test changing priority of ready stream.
  EXPECT_EQ(1u, peer_.NumReadyStreams(5));
  scheduler_.UpdateStreamPriority(1, 6);
  EXPECT_EQ(6, scheduler_.GetStreamPriority(1));
  EXPECT_EQ(0u, peer_.NumReadyStreams(5));
  EXPECT_EQ(1u, peer_.NumReadyStreams(6));

  EXPECT_EQ(1u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(6, scheduler_.GetStreamPriority(1));

  scheduler_.UnregisterStream(1);
  EXPECT_SPDY_BUG(EXPECT_EQ(kV3LowestPriority, scheduler_.GetStreamPriority(1)),
                  "Stream 1 not registered");
}

TEST_F(PriorityWriteSchedulerTest, UpdateStreamPriority) {
  // Updating priority of unregistered stream should have no effect.
  EXPECT_SPDY_BUG(EXPECT_EQ(kV3LowestPriority, scheduler_.GetStreamPriority(3)),
                  "Stream 3 not registered");
  EXPECT_SPDY_BUG(scheduler_.UpdateStreamPriority(3, 1),
                  "Stream 3 not registered");
  EXPECT_SPDY_BUG(EXPECT_EQ(kV3LowestPriority, scheduler_.GetStreamPriority(3)),
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
  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());

  // Verify that lowering priority of stream 4 causes it to be returned later
  // by PopNextReadyStream().
  scheduler_.MarkStreamReady(3, false);  // priority 2
  scheduler_.MarkStreamReady(4, false);  // priority 1
  scheduler_.UpdateStreamPriority(4, 3);
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());

  scheduler_.UnregisterStream(3);
  EXPECT_SPDY_BUG(scheduler_.UpdateStreamPriority(3, 1),
                  "Stream 3 not registered");
}

TEST_F(PriorityWriteSchedulerTest, GetStreamWeight) {
  EXPECT_SPDY_BUG(
      EXPECT_EQ(kHttp2MinStreamWeight, scheduler_.GetStreamWeight(1)),
      "Stream 1 not registered");

  scheduler_.RegisterStream(1, 3);
  scheduler_.RegisterStream(2, 4);
  EXPECT_EQ(SpdyPriorityToHttp2Weight(3), scheduler_.GetStreamWeight(1));
  EXPECT_EQ(SpdyPriorityToHttp2Weight(4), scheduler_.GetStreamWeight(2));

  scheduler_.UnregisterStream(1);
  EXPECT_SPDY_BUG(
      EXPECT_EQ(kHttp2MinStreamWeight, scheduler_.GetStreamWeight(1)),
      "Stream 1 not registered");
}

TEST_F(PriorityWriteSchedulerTest, UpdateStreamWeight) {
  EXPECT_SPDY_BUG(scheduler_.UpdateStreamWeight(3, 100),
                  "Stream 3 not registered");

  scheduler_.RegisterStream(3, 3);
  EXPECT_EQ(SpdyPriorityToHttp2Weight(3), scheduler_.GetStreamWeight(3));

  scheduler_.UpdateStreamWeight(3, SpdyPriorityToHttp2Weight(4));
  EXPECT_EQ(SpdyPriorityToHttp2Weight(4), scheduler_.GetStreamWeight(3));
  EXPECT_EQ(4, scheduler_.GetStreamPriority(3));

  scheduler_.UnregisterStream(3);
  EXPECT_SPDY_BUG(scheduler_.UpdateStreamWeight(3, 100),
                  "Stream 3 not registered");
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamReadyBack) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_SPDY_BUG(scheduler_.MarkStreamReady(1, false),
                  "Stream 1 not registered");
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_SPDY_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
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

  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5u, scheduler_.PopNextReadyStream());
  EXPECT_SPDY_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                  "No ready streams available");
}

TEST_F(PriorityWriteSchedulerTest, MarkStreamReadyFront) {
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_SPDY_BUG(scheduler_.MarkStreamReady(1, true),
                  "Stream 1 not registered");
  EXPECT_FALSE(scheduler_.HasReadyStreams());
  EXPECT_SPDY_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
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

  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5u, scheduler_.PopNextReadyStream());
  EXPECT_SPDY_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
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

  EXPECT_EQ(6u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(4u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(2u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(3u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(1u, scheduler_.PopNextReadyStream());
  EXPECT_EQ(5u, scheduler_.PopNextReadyStream());
  EXPECT_SPDY_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
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
  EXPECT_SPDY_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
                  "No ready streams available");

  // Tolerate redundant marking of a stream as not ready.
  scheduler_.MarkStreamNotReady(1);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());

  // Should only be able to mark registered streams.
  EXPECT_SPDY_BUG(scheduler_.MarkStreamNotReady(3), "Stream 3 not registered");
}

TEST_F(PriorityWriteSchedulerTest, UnregisterRemovesStream) {
  scheduler_.RegisterStream(3, 4);
  scheduler_.MarkStreamReady(3, false);
  EXPECT_EQ(1u, scheduler_.NumReadyStreams());

  // Unregistering a stream should remove it from set of ready streams.
  scheduler_.UnregisterStream(3);
  EXPECT_EQ(0u, scheduler_.NumReadyStreams());
  EXPECT_SPDY_BUG(EXPECT_EQ(0u, scheduler_.PopNextReadyStream()),
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

TEST_F(PriorityWriteSchedulerTest, GetLatestEventWithPrecedence) {
  EXPECT_SPDY_BUG(scheduler_.RecordStreamEventTime(3, 5),
                  "Stream 3 not registered");
  EXPECT_SPDY_BUG(EXPECT_EQ(0, scheduler_.GetLatestEventWithPrecedence(4)),
                  "Stream 4 not registered");

  for (int i = 1; i < 5; ++i) {
    scheduler_.RegisterStream(i, i);
  }
  for (int i = 1; i < 5; ++i) {
    EXPECT_EQ(0, scheduler_.GetLatestEventWithPrecedence(i));
  }
  for (int i = 1; i < 5; ++i) {
    scheduler_.RecordStreamEventTime(i, i * 100);
  }
  for (int i = 1; i < 5; ++i) {
    EXPECT_EQ((i - 1) * 100, scheduler_.GetLatestEventWithPrecedence(i));
  }
}

}  // namespace
}  // namespace test
}  // namespace net
