// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/passive_log_collector.h"

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/string_util.h"
#include "net/url_request/url_request_netlog_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef PassiveLogCollector::RequestTracker RequestTracker;
typedef PassiveLogCollector::SourceInfoList SourceInfoList;
typedef PassiveLogCollector::SocketTracker SocketTracker;
using net::NetLog;

const NetLog::SourceType kSourceType = NetLog::SOURCE_NONE;

PassiveLogCollector::Entry MakeStartLogEntryWithURL(int source_id,
                                                    const std::string& url) {
  return PassiveLogCollector::Entry(
      0,
      NetLog::TYPE_URL_REQUEST_START_JOB,
      base::TimeTicks(),
      NetLog::Source(kSourceType, source_id),
      NetLog::PHASE_BEGIN,
      new URLRequestStartEventParameters(GURL(url), "GET", 0, net::LOW));
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
  collector->OnAddEntry(NetLog::TYPE_URL_REQUEST_START_JOB, base::TimeTicks(),
                        NetLog::Source(NetLog::SOURCE_URL_REQUEST, id),
                        NetLog::PHASE_BEGIN, new URLRequestStartEventParameters(
                            GURL(StringPrintf("http://req%d", id)), "GET", 0,
                            net::LOW));
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

bool OrderBySourceID(const PassiveLogCollector::SourceInfo& a,
                     const PassiveLogCollector::SourceInfo& b) {
  return a.source_id < b.source_id;
}

SourceInfoList GetLiveSources(
    const PassiveLogCollector::SourceTracker& tracker) {
  SourceInfoList result = tracker.GetAllDeadOrAliveSources(true);
  std::sort(result.begin(), result.end(), &OrderBySourceID);
  return result;
}

SourceInfoList GetDeadSources(
    const PassiveLogCollector::SourceTracker& tracker) {
  SourceInfoList result = tracker.GetAllDeadOrAliveSources(false);
  std::sort(result.begin(), result.end(), &OrderBySourceID);
  return result;
}

static const int kMaxNumLoadLogEntries = 1;

}  // namespace

// Test that once the tracker contains a total maximum amount of data
// (graveyard + live requests), it resets itself to avoid growing unbounded.
TEST(RequestTrackerTest, DropsAfterMaximumSize) {
  RequestTracker tracker(NULL);

  // Fill the source tracker with as many sources as it can hold.
  for (size_t i = 0; i < RequestTracker::kMaxNumSources; ++i)
    tracker.OnAddEntry(MakeStartLogEntry(i));

  EXPECT_EQ(RequestTracker::kMaxNumSources, GetLiveSources(tracker).size());

  // Add 1 more -- this should cause it to exceed its expected peak, and
  // therefore reset all of its data.
  tracker.OnAddEntry(
      MakeStartLogEntry(1 + RequestTracker::kMaxNumSources));

  EXPECT_EQ(1u, GetLiveSources(tracker).size());
}

TEST(RequestTrackerTest, BasicBounded) {
  RequestTracker tracker(NULL);
  EXPECT_EQ(0u, GetLiveSources(tracker).size());
  EXPECT_EQ(0u, GetDeadSources(tracker).size());

  tracker.OnAddEntry(MakeStartLogEntry(1));
  tracker.OnAddEntry(MakeStartLogEntry(2));
  tracker.OnAddEntry(MakeStartLogEntry(3));
  tracker.OnAddEntry(MakeStartLogEntry(4));
  tracker.OnAddEntry(MakeStartLogEntry(5));

  SourceInfoList live_reqs = GetLiveSources(tracker);

  ASSERT_EQ(5u, live_reqs.size());
  EXPECT_EQ("http://req1/", live_reqs[0].GetURL());
  EXPECT_EQ("http://req2/", live_reqs[1].GetURL());
  EXPECT_EQ("http://req3/", live_reqs[2].GetURL());
  EXPECT_EQ("http://req4/", live_reqs[3].GetURL());
  EXPECT_EQ("http://req5/", live_reqs[4].GetURL());

  tracker.OnAddEntry(MakeEndLogEntry(1));
  tracker.OnAddEntry(MakeEndLogEntry(5));
  tracker.OnAddEntry(MakeEndLogEntry(3));

  ASSERT_EQ(3u, GetDeadSources(tracker).size());

  live_reqs = GetLiveSources(tracker);

  ASSERT_EQ(2u, live_reqs.size());
  EXPECT_EQ("http://req2/", live_reqs[0].GetURL());
  EXPECT_EQ("http://req4/", live_reqs[1].GetURL());
}

TEST(RequestTrackerTest, GraveyardBounded) {
  RequestTracker tracker(NULL);
  EXPECT_EQ(0u, GetLiveSources(tracker).size());
  EXPECT_EQ(0u, GetDeadSources(tracker).size());

  // Add twice as many requests as will fit in the graveyard.
  for (size_t i = 0; i < RequestTracker::kMaxGraveyardSize * 2; ++i) {
    tracker.OnAddEntry(MakeStartLogEntry(i));
    tracker.OnAddEntry(MakeEndLogEntry(i));
  }

  EXPECT_EQ(0u, GetLiveSources(tracker).size());

  // Check that only the last |kMaxGraveyardSize| requests are in-memory.

  SourceInfoList recent = GetDeadSources(tracker);

  ASSERT_EQ(RequestTracker::kMaxGraveyardSize, recent.size());

  for (size_t i = 0; i < RequestTracker::kMaxGraveyardSize; ++i) {
    size_t req_number = i + RequestTracker::kMaxGraveyardSize;
    std::string url = StringPrintf("http://req%" PRIuS "/", req_number);
    EXPECT_EQ(url, recent[i].GetURL());
  }
}

// Check that we exclude "chrome://" URLs from being saved into the recent
// requests list (graveyard).
TEST(RequestTrackerTest, GraveyardIsFiltered) {
  RequestTracker tracker(NULL);

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

  ASSERT_EQ(2u, GetDeadSources(tracker).size());
  EXPECT_EQ(url2, GetDeadSources(tracker)[0].GetURL());
  EXPECT_EQ(url3, GetDeadSources(tracker)[1].GetURL());
}

TEST(SpdySessionTracker, MovesToGraveyard) {
  PassiveLogCollector::SpdySessionTracker tracker;
  EXPECT_EQ(0u, GetLiveSources(tracker).size());
  EXPECT_EQ(0u, GetDeadSources(tracker).size());

  PassiveLogCollector::Entry begin(
      0u,
      NetLog::TYPE_SPDY_SESSION,
      base::TimeTicks(),
      NetLog::Source(NetLog::SOURCE_SPDY_SESSION, 1),
      NetLog::PHASE_BEGIN,
      NULL);

  tracker.OnAddEntry(begin);
  EXPECT_EQ(1u, GetLiveSources(tracker).size());
  EXPECT_EQ(0u, GetDeadSources(tracker).size());

  PassiveLogCollector::Entry end(
      0u,
      NetLog::TYPE_SPDY_SESSION,
      base::TimeTicks(),
      NetLog::Source(NetLog::SOURCE_SPDY_SESSION, 1),
      NetLog::PHASE_END,
      NULL);

  tracker.OnAddEntry(end);
  EXPECT_EQ(0u, GetLiveSources(tracker).size());
  EXPECT_EQ(1u, GetDeadSources(tracker).size());
}

// Test that when a SOURCE_SOCKET is connected to a SOURCE_URL_REQUEST
// (via the TYPE_SOCKET_POOL_BOUND_TO_SOCKET event), it holds a reference
// to the SOURCE_SOCKET preventing it from getting deleted as long as the
// SOURCE_URL_REQUEST is still around.
TEST(PassiveLogCollectorTest, HoldReferenceToDependentSource) {
  PassiveLogCollector log;

  EXPECT_EQ(0u, GetLiveSources(log.url_request_tracker_).size());
  EXPECT_EQ(0u, GetLiveSources(log.socket_tracker_).size());

  uint32 next_id = 0;
  NetLog::Source socket_source(NetLog::SOURCE_SOCKET, next_id++);
  NetLog::Source url_request_source(NetLog::SOURCE_URL_REQUEST, next_id++);

  // Start a SOURCE_SOCKET.
  log.OnAddEntry(NetLog::TYPE_SOCKET_ALIVE,
                 base::TimeTicks(),
                 socket_source,
                 NetLog::PHASE_BEGIN,
                 NULL);

  EXPECT_EQ(0u, GetLiveSources(log.url_request_tracker_).size());
  EXPECT_EQ(1u, GetLiveSources(log.socket_tracker_).size());

  // Start a SOURCE_URL_REQUEST.
  log.OnAddEntry(NetLog::TYPE_REQUEST_ALIVE,
                 base::TimeTicks(),
                 url_request_source,
                 NetLog::PHASE_BEGIN,
                 NULL);

  // Check that there is no association between the SOURCE_URL_REQUEST and the
  // SOURCE_SOCKET yet.
  ASSERT_EQ(1u, GetLiveSources(log.url_request_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetLiveSources(log.url_request_tracker_)[0];
    EXPECT_EQ(0, info.reference_count);
    EXPECT_EQ(0u, info.dependencies.size());
  }
  ASSERT_EQ(1u, GetLiveSources(log.socket_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetLiveSources(log.socket_tracker_)[0];
    EXPECT_EQ(0, info.reference_count);
    EXPECT_EQ(0u, info.dependencies.size());
  }

  // Associate the SOURCE_SOCKET with the SOURCE_URL_REQUEST.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
                 base::TimeTicks(),
                 url_request_source,
                 NetLog::PHASE_NONE,
                 new net::NetLogSourceParameter("x", socket_source));

  // Check that an associate was made -- the SOURCE_URL_REQUEST should have
  // added a reference to the SOURCE_SOCKET.
  ASSERT_EQ(1u, GetLiveSources(log.url_request_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetLiveSources(log.url_request_tracker_)[0];
    EXPECT_EQ(0, info.reference_count);
    EXPECT_EQ(1u, info.dependencies.size());
    EXPECT_EQ(socket_source.id, info.dependencies[0].id);
  }
  ASSERT_EQ(1u, GetLiveSources(log.socket_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetLiveSources(log.socket_tracker_)[0];
    EXPECT_EQ(1, info.reference_count);
    EXPECT_EQ(0u, info.dependencies.size());
  }

  // Now end both |source_socket| and |source_url_request|. This sends them
  // to deletion queue, and they will be deleted once space runs out.

  log.OnAddEntry(NetLog::TYPE_REQUEST_ALIVE,
                 base::TimeTicks(),
                 url_request_source,
                 NetLog::PHASE_END,
                 NULL);

  log.OnAddEntry(NetLog::TYPE_SOCKET_ALIVE,
                 base::TimeTicks(),
                 socket_source,
                 NetLog::PHASE_END,
                 NULL);

  // Verify that both sources are in fact dead, and that |source_url_request|
  // still holds a reference to |source_socket|.
  ASSERT_EQ(0u, GetLiveSources(log.url_request_tracker_).size());
  ASSERT_EQ(1u, GetDeadSources(log.url_request_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetDeadSources(log.url_request_tracker_)[0];
    EXPECT_EQ(0, info.reference_count);
    EXPECT_EQ(1u, info.dependencies.size());
    EXPECT_EQ(socket_source.id, info.dependencies[0].id);
  }
  EXPECT_EQ(0u, GetLiveSources(log.socket_tracker_).size());
  ASSERT_EQ(1u, GetDeadSources(log.socket_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetDeadSources(log.socket_tracker_)[0];
    EXPECT_EQ(1, info.reference_count);
    EXPECT_EQ(0u, info.dependencies.size());
  }

  // Cycle through a bunch of SOURCE_SOCKET -- if it were not referenced, this
  // loop will have deleted it.
  for (size_t i = 0; i < SocketTracker::kMaxGraveyardSize; ++i) {
      log.OnAddEntry(NetLog::TYPE_SOCKET_ALIVE,
                     base::TimeTicks(),
                     NetLog::Source(NetLog::SOURCE_SOCKET, next_id++),
                     NetLog::PHASE_END,
                     NULL);
  }

  EXPECT_EQ(0u, GetLiveSources(log.socket_tracker_).size());
  ASSERT_EQ(SocketTracker::kMaxGraveyardSize + 1,
            GetDeadSources(log.socket_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetDeadSources(log.socket_tracker_)[0];
    EXPECT_EQ(socket_source.id, info.source_id);
    EXPECT_EQ(1, info.reference_count);
    EXPECT_EQ(0u, info.dependencies.size());
  }

  // Cycle through a bunch of SOURCE_URL_REQUEST -- this will cause
  // |source_url_request| to be freed, which in turn should release the final
  // reference to |source_socket| cause it to be freed as well.
  for (size_t i = 0; i < RequestTracker::kMaxGraveyardSize; ++i) {
    log.OnAddEntry(NetLog::TYPE_REQUEST_ALIVE,
                   base::TimeTicks(),
                   NetLog::Source(NetLog::SOURCE_URL_REQUEST, next_id++),
                   NetLog::PHASE_END,
                   NULL);
  }

  EXPECT_EQ(0u, GetLiveSources(log.url_request_tracker_).size());
  EXPECT_EQ(RequestTracker::kMaxGraveyardSize,
            GetDeadSources(log.url_request_tracker_).size());

  EXPECT_EQ(0u, GetLiveSources(log.socket_tracker_).size());
  EXPECT_EQ(SocketTracker::kMaxGraveyardSize,
            GetDeadSources(log.socket_tracker_).size());
}

// Have a URL_REQUEST hold a reference to a SOCKET. Then cause the SOCKET to
// get evicted (by exceeding maximum sources limit). Now the URL_REQUEST is
// referencing a non-existant SOCKET. Lastly, evict the URL_REQUEST so it
// tries to drop all of its references. Make sure that in releasing its
// non-existant reference it doesn't trip any DCHECKs.
TEST(PassiveLogCollectorTest, HoldReferenceToDeletedSource) {
  PassiveLogCollector log;

  EXPECT_EQ(0u, GetLiveSources(log.url_request_tracker_).size());
  EXPECT_EQ(0u, GetLiveSources(log.socket_tracker_).size());

  uint32 next_id = 0;
  NetLog::Source socket_source(NetLog::SOURCE_SOCKET, next_id++);
  NetLog::Source url_request_source(NetLog::SOURCE_URL_REQUEST, next_id++);

  // Start a SOURCE_SOCKET.
  log.OnAddEntry(NetLog::TYPE_SOCKET_ALIVE,
                 base::TimeTicks(),
                 socket_source,
                 NetLog::PHASE_BEGIN,
                 NULL);

  EXPECT_EQ(0u, GetLiveSources(log.url_request_tracker_).size());
  EXPECT_EQ(1u, GetLiveSources(log.socket_tracker_).size());

  // Start a SOURCE_URL_REQUEST.
  log.OnAddEntry(NetLog::TYPE_REQUEST_ALIVE,
                 base::TimeTicks(),
                 url_request_source,
                 NetLog::PHASE_BEGIN,
                 NULL);

  // Associate the SOURCE_SOCKET with the SOURCE_URL_REQUEST.
  log.OnAddEntry(NetLog::TYPE_SOCKET_POOL_BOUND_TO_SOCKET,
                 base::TimeTicks(),
                 url_request_source,
                 NetLog::PHASE_NONE,
                 new net::NetLogSourceParameter("x", socket_source));

  // Check that an associate was made -- the SOURCE_URL_REQUEST should have
  // added a reference to the SOURCE_SOCKET.
  ASSERT_EQ(1u, GetLiveSources(log.url_request_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetLiveSources(log.url_request_tracker_)[0];
    EXPECT_EQ(0, info.reference_count);
    EXPECT_EQ(1u, info.dependencies.size());
    EXPECT_EQ(socket_source.id, info.dependencies[0].id);
  }
  ASSERT_EQ(1u, GetLiveSources(log.socket_tracker_).size());
  {
    PassiveLogCollector::SourceInfo info =
        GetLiveSources(log.socket_tracker_)[0];
    EXPECT_EQ(1, info.reference_count);
    EXPECT_EQ(0u, info.dependencies.size());
  }

  // Add lots of sources to the socket tracker. This is just enough to cause
  // the tracker to reach its peak, and reset all of its data as a safeguard.
  for (size_t i = 0; i < SocketTracker::kMaxNumSources; ++i) {
    log.OnAddEntry(NetLog::TYPE_SOCKET_ALIVE,
                   base::TimeTicks(),
                   NetLog::Source(NetLog::SOURCE_SOCKET, next_id++),
                   NetLog::PHASE_BEGIN,
                   NULL);
  }
  ASSERT_EQ(1u, GetLiveSources(log.socket_tracker_).size());

  // End the original request. Then saturate the graveyard with enough other
  // requests to cause it to be deleted. Once that source is deleted, it will
  // try to give up its reference to the SOCKET. However that socket_id no
  // longer exists -- should not DCHECK().
  log.OnAddEntry(NetLog::TYPE_REQUEST_ALIVE,
                 base::TimeTicks(),
                 url_request_source,
                 NetLog::PHASE_END,
                 NULL);
  for (size_t i = 0; i < RequestTracker::kMaxGraveyardSize; ++i) {
    log.OnAddEntry(NetLog::TYPE_REQUEST_ALIVE,
                   base::TimeTicks(),
                   NetLog::Source(NetLog::SOURCE_URL_REQUEST, next_id++),
                   NetLog::PHASE_END,
                   NULL);
  }
  EXPECT_EQ(RequestTracker::kMaxGraveyardSize,
            GetDeadSources(log.url_request_tracker_).size());
}

