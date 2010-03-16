// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/passive_log_collector.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef PassiveLogCollector::RequestTracker RequestTracker;
typedef PassiveLogCollector::RequestInfoList RequestInfoList;

const net::NetLog::SourceType kSourceType = net::NetLog::SOURCE_NONE;

net::NetLog::Entry MakeStartLogEntryWithURL(int source_id,
                                            const std::string& url) {
  net::NetLog::Entry entry;
  entry.source.type = kSourceType;
  entry.source.id = source_id;
  entry.type = net::NetLog::Entry::TYPE_EVENT;
  entry.event = net::NetLog::Event(net::NetLog::TYPE_REQUEST_ALIVE,
                                   net::NetLog::PHASE_BEGIN);
  entry.string = url;
  return entry;
}

net::NetLog::Entry MakeStartLogEntry(int source_id) {
  return MakeStartLogEntryWithURL(source_id,
                                  StringPrintf("http://req%d", source_id));
}

net::NetLog::Entry MakeEndLogEntry(int source_id) {
  net::NetLog::Entry entry;
  entry.source.type = kSourceType;
  entry.source.id = source_id;
  entry.type = net::NetLog::Entry::TYPE_EVENT;
  entry.event = net::NetLog::Event(net::NetLog::TYPE_REQUEST_ALIVE,
                                   net::NetLog::PHASE_END);
  return entry;
}

static const int kMaxNumLoadLogEntries = 1;

TEST(RequestTrackerTest, BasicBounded) {
  RequestTracker tracker(NULL);
  EXPECT_FALSE(tracker.IsUnbounded());
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  tracker.OnAddEntry(MakeStartLogEntry(1));
  tracker.OnAddEntry(MakeStartLogEntry(2));
  tracker.OnAddEntry(MakeStartLogEntry(3));
  tracker.OnAddEntry(MakeStartLogEntry(4));
  tracker.OnAddEntry(MakeStartLogEntry(5));

  RequestInfoList live_reqs = tracker.GetLiveRequests();

  ASSERT_EQ(5u, live_reqs.size());
  EXPECT_EQ("http://req1", live_reqs[0].url);
  EXPECT_EQ("http://req2", live_reqs[1].url);
  EXPECT_EQ("http://req3", live_reqs[2].url);
  EXPECT_EQ("http://req4", live_reqs[3].url);
  EXPECT_EQ("http://req5", live_reqs[4].url);

  tracker.OnAddEntry(MakeEndLogEntry(1));
  tracker.OnAddEntry(MakeEndLogEntry(5));
  tracker.OnAddEntry(MakeEndLogEntry(3));

  ASSERT_EQ(3u, tracker.GetRecentlyDeceased().size());

  live_reqs = tracker.GetLiveRequests();

  ASSERT_EQ(2u, live_reqs.size());
  EXPECT_EQ("http://req2", live_reqs[0].url);
  EXPECT_EQ("http://req4", live_reqs[1].url);
}

TEST(RequestTrackerTest, GraveyardBounded) {
  RequestTracker tracker(NULL);
  EXPECT_FALSE(tracker.IsUnbounded());
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  // Add twice as many requests as will fit in the graveyard.
  for (size_t i = 0; i < RequestTracker::kMaxGraveyardSize * 2; ++i) {
    tracker.OnAddEntry(MakeStartLogEntry(i));
    tracker.OnAddEntry(MakeEndLogEntry(i));
  }

  // Check that only the last |kMaxGraveyardSize| requests are in-memory.

  RequestInfoList recent_reqs = tracker.GetRecentlyDeceased();

  ASSERT_EQ(RequestTracker::kMaxGraveyardSize, recent_reqs.size());

  for (size_t i = 0; i < RequestTracker::kMaxGraveyardSize; ++i) {
    size_t req_number = i + RequestTracker::kMaxGraveyardSize;
    std::string url = StringPrintf("http://req%" PRIuS, req_number);
    EXPECT_EQ(url, recent_reqs[i].url);
  }
}

TEST(RequestTrackerTest, GraveyardUnbounded) {
  RequestTracker tracker(NULL);
  EXPECT_FALSE(tracker.IsUnbounded());
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  tracker.SetUnbounded(true);

  EXPECT_TRUE(tracker.IsUnbounded());

  // Add twice as many requests as would fit in the bounded graveyard.

  size_t kMaxSize = RequestTracker::kMaxGraveyardSize * 2;
  for (size_t i = 0; i < kMaxSize; ++i) {
    tracker.OnAddEntry(MakeStartLogEntry(i));
    tracker.OnAddEntry(MakeEndLogEntry(i));
  }

  // Check that all of them got saved.

  RequestInfoList recent_reqs = tracker.GetRecentlyDeceased();

  ASSERT_EQ(kMaxSize, recent_reqs.size());

  for (size_t i = 0; i < kMaxSize; ++i) {
    std::string url = StringPrintf("http://req%" PRIuS, i);
    EXPECT_EQ(url, recent_reqs[i].url);
  }
}

// Check that very long URLs are truncated.
TEST(RequestTrackerTest, GraveyardURLBounded) {
  RequestTracker tracker(NULL);
  EXPECT_FALSE(tracker.IsUnbounded());

  std::string big_url("http://");
  big_url.resize(2 * RequestTracker::kMaxGraveyardURLSize, 'x');

  tracker.OnAddEntry(MakeStartLogEntryWithURL(1, big_url));
  tracker.OnAddEntry(MakeEndLogEntry(1));

  ASSERT_EQ(1u, tracker.GetRecentlyDeceased().size());
  EXPECT_EQ(RequestTracker::kMaxGraveyardURLSize,
            tracker.GetRecentlyDeceased()[0].url.size());
}

// Check that we exclude "chrome://" URLs from being saved into the recent
// requests list (graveyard).
TEST(RequestTrackerTest, GraveyardIsFiltered) {
  RequestTracker tracker(NULL);
  EXPECT_FALSE(tracker.IsUnbounded());

  // This will be excluded.
  std::string url1 = "chrome://dontcare/";
  tracker.OnAddEntry(MakeStartLogEntryWithURL(1, url1));
  tracker.OnAddEntry(MakeEndLogEntry(1));

  // This will be be added to graveyard.
  std::string url2 = "chrome2://dontcare/";
  tracker.OnAddEntry(MakeStartLogEntryWithURL(2, url2));
  tracker.OnAddEntry(MakeEndLogEntry(2));

  // This will be be added to graveyard.
  std::string url3 = "http://foo/";
  tracker.OnAddEntry(MakeStartLogEntryWithURL(3, url3));
  tracker.OnAddEntry(MakeEndLogEntry(3));

  ASSERT_EQ(2u, tracker.GetRecentlyDeceased().size());
  EXPECT_EQ(url2, tracker.GetRecentlyDeceased()[0].url);
  EXPECT_EQ(url3, tracker.GetRecentlyDeceased()[1].url);
}

// Convert an unbounded tracker back to being bounded.
TEST(RequestTrackerTest, ConvertUnboundedToBounded) {
  RequestTracker tracker(NULL);
  EXPECT_FALSE(tracker.IsUnbounded());
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  tracker.SetUnbounded(true);
  EXPECT_TRUE(tracker.IsUnbounded());

  // Add twice as many requests as would fit in the bounded graveyard.

  size_t kMaxSize = RequestTracker::kMaxGraveyardSize * 2;
  for (size_t i = 0; i < kMaxSize; ++i) {
    tracker.OnAddEntry(MakeStartLogEntry(i));
    tracker.OnAddEntry(MakeEndLogEntry(i));
  }

  // Check that all of them got saved.
  ASSERT_EQ(kMaxSize, tracker.GetRecentlyDeceased().size());

  // Now make the tracker bounded, and add more entries to its graveyard.
  tracker.SetUnbounded(false);

  kMaxSize = RequestTracker::kMaxGraveyardSize;
  for (size_t i = kMaxSize; i < 2 * kMaxSize; ++i) {
    tracker.OnAddEntry(MakeStartLogEntry(i));
    tracker.OnAddEntry(MakeEndLogEntry(i));
  }

  // We should only have kMaxGraveyardSize entries now.
  ASSERT_EQ(kMaxSize, tracker.GetRecentlyDeceased().size());
}

}  // namespace
