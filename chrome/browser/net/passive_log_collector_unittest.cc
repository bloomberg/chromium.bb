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
typedef net::NetLog NetLog;

const NetLog::SourceType kSourceType = NetLog::SOURCE_NONE;

PassiveLogCollector::Entry MakeStartLogEntryWithURL(int source_id,
                                                    const std::string& url) {
  return PassiveLogCollector::Entry(
      0,
      NetLog::TYPE_URL_REQUEST_START,
      base::TimeTicks(),
      NetLog::Source(kSourceType, source_id),
      NetLog::PHASE_BEGIN,
      new net::NetLogStringParameter("url", url));
}

PassiveLogCollector::Entry MakeStartLogEntry(int source_id) {
  return MakeStartLogEntryWithURL(source_id,
                                  StringPrintf("http://req%d", source_id));
}

PassiveLogCollector::Entry MakeEndLogEntry(int source_id) {
  return PassiveLogCollector::Entry(
      0,
      NetLog::TYPE_REQUEST_ALIVE,
      base::TimeTicks(),
      NetLog::Source(kSourceType, source_id),
      NetLog::PHASE_END,
      NULL);
}

void AddStartURLRequestEntries(PassiveLogCollector* collector, uint32 id) {
  collector->OnAddEntry(NetLog::TYPE_REQUEST_ALIVE, base::TimeTicks(),
                        NetLog::Source(NetLog::SOURCE_URL_REQUEST, id),
                        NetLog::PHASE_BEGIN, NULL);
  collector->OnAddEntry(NetLog::TYPE_URL_REQUEST_START, base::TimeTicks(),
                        NetLog::Source(NetLog::SOURCE_URL_REQUEST, id),
                        NetLog::PHASE_BEGIN, new net::NetLogStringParameter(
                            "url", StringPrintf("http://req%d", id)));
}

void AddEndURLRequestEntries(PassiveLogCollector* collector, uint32 id) {
  collector->OnAddEntry(NetLog::TYPE_REQUEST_ALIVE, base::TimeTicks(),
                        NetLog::Source(NetLog::SOURCE_URL_REQUEST, id),
                        NetLog::PHASE_END, NULL);
}

std::string GetStringParam(const PassiveLogCollector::Entry& entry) {
  return static_cast<net::NetLogStringParameter*>(
      entry.params.get())->value();
}

static const int kMaxNumLoadLogEntries = 1;

}  // namespace

TEST(RequestTrackerTest, BasicBounded) {
  RequestTracker tracker(NULL, NULL);
  EXPECT_FALSE(tracker.is_unbounded());
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  tracker.OnAddEntry(MakeStartLogEntry(1));
  tracker.OnAddEntry(MakeStartLogEntry(2));
  tracker.OnAddEntry(MakeStartLogEntry(3));
  tracker.OnAddEntry(MakeStartLogEntry(4));
  tracker.OnAddEntry(MakeStartLogEntry(5));

  RequestInfoList live_reqs = tracker.GetLiveRequests();

  ASSERT_EQ(5u, live_reqs.size());
  EXPECT_EQ("http://req1", live_reqs[0].GetURL());
  EXPECT_EQ("http://req2", live_reqs[1].GetURL());
  EXPECT_EQ("http://req3", live_reqs[2].GetURL());
  EXPECT_EQ("http://req4", live_reqs[3].GetURL());
  EXPECT_EQ("http://req5", live_reqs[4].GetURL());

  tracker.OnAddEntry(MakeEndLogEntry(1));
  tracker.OnAddEntry(MakeEndLogEntry(5));
  tracker.OnAddEntry(MakeEndLogEntry(3));

  ASSERT_EQ(3u, tracker.GetRecentlyDeceased().size());

  live_reqs = tracker.GetLiveRequests();

  ASSERT_EQ(2u, live_reqs.size());
  EXPECT_EQ("http://req2", live_reqs[0].GetURL());
  EXPECT_EQ("http://req4", live_reqs[1].GetURL());
}

TEST(RequestTrackerTest, GraveyardBounded) {
  RequestTracker tracker(NULL, NULL);
  EXPECT_FALSE(tracker.is_unbounded());
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
    EXPECT_EQ(url, recent_reqs[i].GetURL());
  }
}

TEST(RequestTrackerTest, GraveyardUnbounded) {
  RequestTracker tracker(NULL, NULL);
  EXPECT_FALSE(tracker.is_unbounded());
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  tracker.SetUnbounded(true);

  EXPECT_TRUE(tracker.is_unbounded());

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
    EXPECT_EQ(url, recent_reqs[i].GetURL());
  }
}

// Check that we exclude "chrome://" URLs from being saved into the recent
// requests list (graveyard).
TEST(RequestTrackerTest, GraveyardIsFiltered) {
  RequestTracker tracker(NULL, NULL);
  EXPECT_FALSE(tracker.is_unbounded());

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
  EXPECT_EQ(url2, tracker.GetRecentlyDeceased()[0].GetURL());
  EXPECT_EQ(url3, tracker.GetRecentlyDeceased()[1].GetURL());
}

// Convert an unbounded tracker back to being bounded.
TEST(RequestTrackerTest, ConvertUnboundedToBounded) {
  RequestTracker tracker(NULL, NULL);
  EXPECT_FALSE(tracker.is_unbounded());
  EXPECT_EQ(0u, tracker.GetLiveRequests().size());
  EXPECT_EQ(0u, tracker.GetRecentlyDeceased().size());

  tracker.SetUnbounded(true);
  EXPECT_TRUE(tracker.is_unbounded());

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

TEST(PassiveLogCollectorTest, BasicConnectJobAssociation) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);
  AddStartURLRequestEntries(&log, 20);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(10u, requests[0].source_id);
  EXPECT_EQ(2u, requests[0].entries.size());
  EXPECT_EQ(20u, requests[1].source_id);
  EXPECT_EQ(2u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_BEGIN, NULL);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 21));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(4u, requests[0].entries.size());
  EXPECT_EQ(5u, requests[1].entries.size());

  AddEndURLRequestEntries(&log, 10);
  AddEndURLRequestEntries(&log, 20);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(2u, requests.size());
  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(6u, requests[1].entries.size());
}

TEST(PassiveLogCollectorTest, BasicSocketAssociation) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);
  AddStartURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_END, NULL);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 21));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(6u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 21));

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(6u, requests[0].entries.size());
  EXPECT_EQ(7u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 25));

  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(8u, requests[0].entries.size());
  EXPECT_EQ(10u, requests[1].entries.size());

  AddEndURLRequestEntries(&log, 10);
  AddEndURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(2u, requests.size());
  EXPECT_EQ(9u, requests[0].entries.size());
  EXPECT_EQ(11u, requests[1].entries.size());
}

TEST(PassiveLogCollectorTest, IdleSocketAssociation) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);
  AddStartURLRequestEntries(&log, 20);
  log.OnAddEntry(NetLog::TYPE_INIT_PROXY_RESOLVER , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(2u, requests[0].entries.size());
  EXPECT_EQ(3u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 25));

  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(4u, requests[0].entries.size());
  EXPECT_EQ(6u, requests[1].entries.size());

  AddEndURLRequestEntries(&log, 10);
  AddEndURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(2u, requests.size());
  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(7u, requests[1].entries.size());
}

TEST(PassiveLogCollectorTest, IdleAssociateAfterConnectJobStarted) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);
  AddStartURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_END, NULL);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 21));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(6u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 25));

  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(6u, requests[1].entries.size());

  AddEndURLRequestEntries(&log, 10);
  AddEndURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(2u, requests.size());
  EXPECT_EQ(6u, requests[0].entries.size());
  EXPECT_EQ(7u, requests[1].entries.size());
}

TEST(PassiveLogCollectorTest, LateBindDifferentConnectJob) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);
  AddStartURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_END, NULL);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 21));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_END, NULL);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 31),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 31),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 31),
                 NetLog::PHASE_END, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 31),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(6u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 21));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 31));

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(7u, requests[0].entries.size());
  EXPECT_EQ(8u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 25));

  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(9u, requests[0].entries.size());
  EXPECT_EQ(11u, requests[1].entries.size());

  AddEndURLRequestEntries(&log, 10);
  AddEndURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(2u, requests.size());
  EXPECT_EQ(10u, requests[0].entries.size());
  EXPECT_EQ(12u, requests[1].entries.size());
}

TEST(PassiveLogCollectorTest, LateBindPendingConnectJob) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);
  AddStartURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_END, NULL);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 21),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(2u, requests[0].entries.size());
  EXPECT_EQ(2u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 21));

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(6u, requests[1].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 25));

  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(2u, requests.size());

  EXPECT_EQ(7u, requests[0].entries.size());
  EXPECT_EQ(9u, requests[1].entries.size());

  AddEndURLRequestEntries(&log, 10);
  AddEndURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 25),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(2u, requests.size());
  EXPECT_EQ(8u, requests[0].entries.size());
  EXPECT_EQ(10u, requests[1].entries.size());
}

TEST(PassiveLogCollectorTest, ReconnectToIdleSocket) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);

  // Initial socket.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(4u, requests[0].entries.size());

  // Reconnect.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 17));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 17),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(6u, requests[0].entries.size());

  AddEndURLRequestEntries(&log, 10);

  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(7u, requests[0].entries.size());
}

TEST(PassiveLogCollectorTest, ReconnectToLateBoundSocket) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);

  // Initial socket.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(4u, requests[0].entries.size());

  // Now reconnect.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_BEGIN, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(6u, requests[0].entries.size());

  // But we get late bound to an idle socket.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 17));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 17),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(7u, requests[0].entries.size());

  AddEndURLRequestEntries(&log, 10);

  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(8u, requests[0].entries.size());
}

TEST(PassiveLogCollectorTest, ReconnectToLateBoundConnectJob) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);

  // Initial socket.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(4u, requests[0].entries.size());

  // Now reconnect.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_BEGIN, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(6u, requests[0].entries.size());

  // But we get late bound to a different ConnectJob.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 12),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 12),
                 NetLog::PHASE_END, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 12));

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(8u, requests[0].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 17));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 17),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(10u, requests[0].entries.size());

  AddEndURLRequestEntries(&log, 10);

  log.OnAddEntry(NetLog::TYPE_TCP_SOCKET_DONE , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(11u, requests[0].entries.size());
}

TEST(PassiveLogCollectorTest, LostConnectJob) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_BEGIN, new net::NetLogIntegerParameter("x", 11));
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_NONE, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_CONNECT_JOB, 11),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(6u, requests[0].entries.size());

  log.connect_job_tracker_.Clear();

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_CONNECT_JOB_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 11));

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(5u, requests[0].entries.size());
}

TEST(PassiveLogCollectorTest, LostSocket) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_END, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_BEGIN, NULL);
  log.OnAddEntry(NetLog::TYPE_SSL_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_END, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(7u, requests[0].entries.size());

  log.socket_tracker_.Clear();

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(4u, requests[0].entries.size());
}

TEST(PassiveLogCollectorTest, AccumulateRxTxData) {
  PassiveLogCollector log;

  RequestInfoList requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());

  AddStartURLRequestEntries(&log, 10);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 10),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(4u, requests[0].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_BYTES_SENT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 1));
  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(NetLog::TYPE_TODO_STRING, requests[0].entries[4].type);
  EXPECT_EQ("Tx/Rx: 1/0 [1/0 total on socket] (Bytes)",
            GetStringParam(requests[0].entries[4]));

  log.OnAddEntry(NetLog::TYPE_SOCKET_BYTES_RECEIVED , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 2));
  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(NetLog::TYPE_TODO_STRING, requests[0].entries[4].type);
  EXPECT_EQ("Tx/Rx: 1/2 [1/2 total on socket] (Bytes)",
            GetStringParam(requests[0].entries[4]));

  AddEndURLRequestEntries(&log, 10);
  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());
  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(6u, requests[0].entries.size());
  EXPECT_EQ(NetLog::TYPE_TODO_STRING, requests[0].entries[4].type);
  EXPECT_EQ("Tx/Rx: 1/2 [1/2 total on socket] (Bytes)",
            GetStringParam(requests[0].entries[4]));

  AddStartURLRequestEntries(&log, 20);

  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_SOCKET_ID , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_URL_REQUEST, 20),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 15));
  log.OnAddEntry(NetLog::TYPE_SOCKS_CONNECT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_NONE, NULL);

  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(4u, requests[0].entries.size());

  log.OnAddEntry(NetLog::TYPE_SOCKET_BYTES_SENT , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 4));
  log.OnAddEntry(NetLog::TYPE_SOCKET_BYTES_RECEIVED , base::TimeTicks(),
                 NetLog::Source(NetLog::SOURCE_SOCKET, 15),
                 NetLog::PHASE_END, new net::NetLogIntegerParameter("x", 8));
  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(1u, requests.size());
  EXPECT_EQ(5u, requests[0].entries.size());
  EXPECT_EQ(NetLog::TYPE_TODO_STRING, requests[0].entries[4].type);
  EXPECT_EQ("Tx/Rx: 4/8 [5/10 total on socket] (Bytes)",
            GetStringParam(requests[0].entries[4]));

  AddEndURLRequestEntries(&log, 20);
  requests = log.url_request_tracker()->GetLiveRequests();
  EXPECT_EQ(0u, requests.size());
  requests = log.url_request_tracker()->GetRecentlyDeceased();
  EXPECT_EQ(2u, requests.size());
  EXPECT_EQ(6u, requests[0].entries.size());
  EXPECT_EQ(6u, requests[1].entries.size());
}
