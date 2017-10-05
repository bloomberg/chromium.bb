// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_client_session_cache.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event_argument.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

namespace {

std::unique_ptr<base::SimpleTestClock> MakeTestClock() {
  std::unique_ptr<base::SimpleTestClock> clock =
      std::make_unique<base::SimpleTestClock>();
  // SimpleTestClock starts at the null base::Time which converts to and from
  // time_t confusingly.
  clock->SetNow(base::Time::FromTimeT(1000000000));
  return clock;
}

class SSLClientSessionCacheTest : public testing::Test {
 public:
  SSLClientSessionCacheTest() : ssl_ctx_(SSL_CTX_new(TLS_method())) {}

 protected:
  bssl::UniquePtr<SSL_SESSION> NewSSLSession(
      uint16_t version = TLS1_2_VERSION) {
    SSL_SESSION* session = SSL_SESSION_new(ssl_ctx_.get());
    if (!SSL_SESSION_set_protocol_version(session, version))
      return nullptr;
    return bssl::UniquePtr<SSL_SESSION>(session);
  }

  bssl::UniquePtr<SSL_SESSION> MakeTestSession(base::Time now,
                                               base::TimeDelta timeout) {
    bssl::UniquePtr<SSL_SESSION> session = NewSSLSession();
    SSL_SESSION_set_time(session.get(), now.ToTimeT());
    SSL_SESSION_set_timeout(session.get(), timeout.InSeconds());
    return session;
  }

 private:
  bssl::UniquePtr<SSL_CTX> ssl_ctx_;
};

}  // namespace

// Test basic insertion and lookup operations.
TEST_F(SSLClientSessionCacheTest, Basic) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session1 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session2 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session3 = NewSSLSession();
  EXPECT_EQ(1u, session1->references);
  EXPECT_EQ(1u, session2->references);
  EXPECT_EQ(1u, session3->references);

  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert("key1", session1.get());
  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(1u, cache.size());

  cache.Insert("key2", session2.get());
  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key2").get());
  EXPECT_EQ(2u, cache.size());

  EXPECT_EQ(2u, session1->references);
  EXPECT_EQ(2u, session2->references);

  cache.Insert("key1", session3.get());
  EXPECT_EQ(session3.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key2").get());
  EXPECT_EQ(2u, cache.size());

  EXPECT_EQ(1u, session1->references);
  EXPECT_EQ(2u, session2->references);
  EXPECT_EQ(2u, session3->references);

  cache.Flush();
  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(nullptr, cache.Lookup("key3").get());
  EXPECT_EQ(0u, cache.size());

  EXPECT_EQ(1u, session1->references);
  EXPECT_EQ(1u, session2->references);
  EXPECT_EQ(1u, session3->references);
}

// Test basic insertion and lookup operations with single-use sessions.
TEST_F(SSLClientSessionCacheTest, BasicSingleUse) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session1 = NewSSLSession(TLS1_3_VERSION);
  bssl::UniquePtr<SSL_SESSION> session2 = NewSSLSession(TLS1_3_VERSION);
  bssl::UniquePtr<SSL_SESSION> session3 = NewSSLSession(TLS1_3_VERSION);
  EXPECT_EQ(1u, session1->references);
  EXPECT_EQ(1u, session2->references);
  EXPECT_EQ(1u, session3->references);

  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert("key1", session1.get());
  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(0u, cache.size());

  EXPECT_EQ(nullptr, cache.Lookup("key1").get());

  cache.Insert("key1", session1.get());
  cache.Insert("key1", session1.get());
  cache.Insert("key2", session2.get());

  EXPECT_EQ(3u, session1->references);
  EXPECT_EQ(2u, session2->references);

  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key2").get());
  EXPECT_EQ(1u, cache.size());

  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());

  EXPECT_EQ(1u, session1->references);
  EXPECT_EQ(1u, session2->references);

  cache.Insert("key1", session1.get());
  cache.Insert("key1", session3.get());
  cache.Insert("key2", session2.get());
  EXPECT_EQ(session3.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key2").get());
  EXPECT_EQ(0u, cache.size());

  EXPECT_EQ(1u, session1->references);
  EXPECT_EQ(1u, session2->references);
  EXPECT_EQ(1u, session3->references);

  cache.Flush();
  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(nullptr, cache.Lookup("key3").get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert("key1", session1.get());
  cache.Insert("key1", session2.get());
  cache.Insert("key1", session3.get());
  EXPECT_EQ(1u, session1->references);
  EXPECT_EQ(2u, session2->references);
  EXPECT_EQ(2u, session3->references);
  EXPECT_EQ(session3.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key1").get());

  EXPECT_EQ(1u, session1->references);
  EXPECT_EQ(1u, session2->references);
  EXPECT_EQ(1u, session3->references);
}

// Test insertion and lookup operations with both single-use and reusable
// sessions.
TEST_F(SSLClientSessionCacheTest, MixedUse) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session_single = NewSSLSession(TLS1_3_VERSION);
  bssl::UniquePtr<SSL_SESSION> session_reuse = NewSSLSession(TLS1_2_VERSION);
  EXPECT_EQ(1u, session_single->references);
  EXPECT_EQ(1u, session_reuse->references);

  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert("key1", session_reuse.get());
  EXPECT_EQ(session_reuse.get(), cache.Lookup("key1").get());
  EXPECT_EQ(1u, cache.size());

  cache.Insert("key1", session_single.get());
  EXPECT_EQ(session_single.get(), cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(0u, cache.size());

  EXPECT_EQ(1u, session_single->references);
  EXPECT_EQ(1u, session_reuse->references);

  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert("key2", session_single.get());
  cache.Insert("key2", session_single.get());
  EXPECT_EQ(1u, cache.size());

  EXPECT_EQ(session_single.get(), cache.Lookup("key2").get());
  EXPECT_EQ(session_single.get(), cache.Lookup("key2").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert("key2", session_single.get());
  cache.Insert("key2", session_reuse.get());
  EXPECT_EQ(session_reuse.get(), cache.Lookup("key2").get());
  EXPECT_EQ(session_reuse.get(), cache.Lookup("key2").get());
  EXPECT_EQ(1u, cache.size());

  EXPECT_EQ(2u, session_single->references);
  EXPECT_EQ(2u, session_reuse->references);
}

// Test that a session may be inserted at two different keys. This should never
// be necessary, but the API doesn't prohibit it.
TEST_F(SSLClientSessionCacheTest, DoubleInsert) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session = NewSSLSession();
  EXPECT_EQ(1u, session->references);

  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(0u, cache.size());

  cache.Insert("key1", session.get());
  EXPECT_EQ(session.get(), cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(1u, cache.size());

  EXPECT_EQ(2u, session->references);

  cache.Insert("key2", session.get());
  EXPECT_EQ(session.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session.get(), cache.Lookup("key2").get());
  EXPECT_EQ(2u, cache.size());

  EXPECT_EQ(3u, session->references);

  cache.Flush();
  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(nullptr, cache.Lookup("key2").get());
  EXPECT_EQ(0u, cache.size());

  EXPECT_EQ(1u, session->references);
}

// Tests that the session cache's size is correctly bounded.
TEST_F(SSLClientSessionCacheTest, MaxEntries) {
  SSLClientSessionCache::Config config;
  config.max_entries = 3;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session1 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session2 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session3 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session4 = NewSSLSession();

  // Insert three entries.
  cache.Insert("key1", session1.get());
  cache.Insert("key2", session2.get());
  cache.Insert("key3", session3.get());
  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key2").get());
  EXPECT_EQ(session3.get(), cache.Lookup("key3").get());
  EXPECT_EQ(3u, cache.size());

  // On insertion of a fourth, the first is removed.
  cache.Insert("key4", session4.get());
  EXPECT_EQ(nullptr, cache.Lookup("key1").get());
  EXPECT_EQ(session4.get(), cache.Lookup("key4").get());
  EXPECT_EQ(session3.get(), cache.Lookup("key3").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key2").get());
  EXPECT_EQ(3u, cache.size());

  // Despite being newest, the next to be removed is session4 as it was accessed
  // least. recently.
  cache.Insert("key1", session1.get());
  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key2").get());
  EXPECT_EQ(session3.get(), cache.Lookup("key3").get());
  EXPECT_EQ(nullptr, cache.Lookup("key4").get());
  EXPECT_EQ(3u, cache.size());
}

// Tests that session expiration works properly.
TEST_F(SSLClientSessionCacheTest, Expiration) {
  const size_t kNumEntries = 20;
  const size_t kExpirationCheckCount = 10;
  const base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(1000);

  SSLClientSessionCache::Config config;
  config.expiration_check_count = kExpirationCheckCount;
  SSLClientSessionCache cache(config);
  base::SimpleTestClock* clock = MakeTestClock().release();
  cache.SetClockForTesting(base::WrapUnique(clock));

  // Add |kNumEntries - 1| entries.
  for (size_t i = 0; i < kNumEntries - 1; i++) {
    bssl::UniquePtr<SSL_SESSION> session =
        MakeTestSession(clock->Now(), kTimeout);
    cache.Insert(base::SizeTToString(i), session.get());
  }
  EXPECT_EQ(kNumEntries - 1, cache.size());

  // Expire all the previous entries and insert one more entry.
  clock->Advance(kTimeout * 2);
  bssl::UniquePtr<SSL_SESSION> session =
      MakeTestSession(clock->Now(), kTimeout);
  cache.Insert("key", session.get());

  // All entries are still in the cache.
  EXPECT_EQ(kNumEntries, cache.size());

  // Perform one fewer lookup than needed to trigger the expiration check. This
  // shall not expire any session.
  for (size_t i = 0; i < kExpirationCheckCount - 1; i++)
    cache.Lookup("key");

  // All entries are still in the cache.
  EXPECT_EQ(kNumEntries, cache.size());

  // Perform one more lookup. This will expire all sessions but the last one.
  cache.Lookup("key");
  EXPECT_EQ(1u, cache.size());
  EXPECT_EQ(session.get(), cache.Lookup("key").get());
  for (size_t i = 0; i < kNumEntries - 1; i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(nullptr, cache.Lookup(base::SizeTToString(i)));
  }
}

// Tests that Lookup performs an expiration check before returning a cached
// session.
TEST_F(SSLClientSessionCacheTest, LookupExpirationCheck) {
  // kExpirationCheckCount is set to a suitably large number so the automated
  // pruning never triggers.
  const size_t kExpirationCheckCount = 1000;
  const base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(1000);

  SSLClientSessionCache::Config config;
  config.expiration_check_count = kExpirationCheckCount;
  SSLClientSessionCache cache(config);
  base::SimpleTestClock* clock = MakeTestClock().release();
  cache.SetClockForTesting(base::WrapUnique(clock));

  // Insert an entry into the session cache.
  bssl::UniquePtr<SSL_SESSION> session =
      MakeTestSession(clock->Now(), kTimeout);
  cache.Insert("key", session.get());
  EXPECT_EQ(session.get(), cache.Lookup("key").get());
  EXPECT_EQ(1u, cache.size());

  // Expire the session.
  clock->Advance(kTimeout * 2);

  // The entry has not been removed yet.
  EXPECT_EQ(1u, cache.size());

  // But it will not be returned on lookup and gets pruned at that point.
  EXPECT_EQ(nullptr, cache.Lookup("key").get());
  EXPECT_EQ(0u, cache.size());

  // Re-inserting a session does not refresh the lifetime. The expiration
  // information in the session is used.
  cache.Insert("key", session.get());
  EXPECT_EQ(nullptr, cache.Lookup("key").get());
  EXPECT_EQ(0u, cache.size());

  // Re-insert a fresh copy of the session.
  session = MakeTestSession(clock->Now(), kTimeout);
  cache.Insert("key", session.get());
  EXPECT_EQ(session.get(), cache.Lookup("key").get());
  EXPECT_EQ(1u, cache.size());

  // Sessions also are treated as expired if the clock rewinds.
  clock->Advance(base::TimeDelta::FromSeconds(-1));
  EXPECT_EQ(nullptr, cache.Lookup("key").get());
  EXPECT_EQ(0u, cache.size());
}

// Test that SSL cache is flushed on low memory notifications
TEST_F(SSLClientSessionCacheTest, TestFlushOnMemoryNotifications) {
  // kExpirationCheckCount is set to a suitably large number so the automated
  // pruning never triggers.
  const size_t kExpirationCheckCount = 1000;
  const base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(1000);

  SSLClientSessionCache::Config config;
  config.expiration_check_count = kExpirationCheckCount;
  SSLClientSessionCache cache(config);
  base::SimpleTestClock* clock = MakeTestClock().release();
  cache.SetClockForTesting(base::WrapUnique(clock));

  // Insert an entry into the session cache.
  bssl::UniquePtr<SSL_SESSION> session1 =
      MakeTestSession(clock->Now(), kTimeout);
  cache.Insert("key1", session1.get());
  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(1u, cache.size());

  // Expire the session.
  clock->Advance(kTimeout * 2);
  // Add one more session.
  bssl::UniquePtr<SSL_SESSION> session2 =
      MakeTestSession(clock->Now(), kTimeout);
  cache.Insert("key2", session2.get());
  EXPECT_EQ(2u, cache.size());

  // Fire a notification that will flush expired sessions.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  base::RunLoop().RunUntilIdle();

  // Expired session's cache should be flushed.
  // Lookup returns nullptr, when cache entry not found.
  EXPECT_FALSE(cache.Lookup("key1"));
  EXPECT_TRUE(cache.Lookup("key2"));
  EXPECT_EQ(1u, cache.size());

  // Fire notification that will flush everything.
  base::MemoryPressureListener::NotifyMemoryPressure(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, cache.size());
}

class SSLClientSessionCacheMemoryDumpTest
    : public SSLClientSessionCacheTest,
      public testing::WithParamInterface<
          base::trace_event::MemoryDumpLevelOfDetail> {};

INSTANTIATE_TEST_CASE_P(
    /* no prefix */,
    SSLClientSessionCacheMemoryDumpTest,
    ::testing::Values(base::trace_event::MemoryDumpLevelOfDetail::DETAILED,
                      base::trace_event::MemoryDumpLevelOfDetail::BACKGROUND));

// Basic test for dumping memory stats.
TEST_P(SSLClientSessionCacheMemoryDumpTest, TestDumpMemoryStats) {
  SSLClientSessionCache::Config config;
  SSLClientSessionCache cache(config);

  bssl::UniquePtr<SSL_SESSION> session1 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session2 = NewSSLSession();
  bssl::UniquePtr<SSL_SESSION> session3 = NewSSLSession();

  // Insert three entries.
  cache.Insert("key1", session1.get());
  cache.Insert("key2", session2.get());
  cache.Insert("key3", session3.get());
  EXPECT_EQ(session1.get(), cache.Lookup("key1").get());
  EXPECT_EQ(session2.get(), cache.Lookup("key2").get());
  EXPECT_EQ(session3.get(), cache.Lookup("key3").get());
  EXPECT_EQ(3u, cache.size());

  base::trace_event::MemoryDumpArgs dump_args = {GetParam()};
  std::unique_ptr<base::trace_event::ProcessMemoryDump> process_memory_dump(
      new base::trace_event::ProcessMemoryDump(nullptr, dump_args));
  cache.DumpMemoryStats(process_memory_dump.get());

  const base::trace_event::MemoryAllocatorDump* dump =
      process_memory_dump->GetAllocatorDump("net/ssl_session_cache");
  ASSERT_NE(nullptr, dump);
  std::unique_ptr<base::Value> raw_attrs =
      dump->attributes_for_testing()->ToBaseValue();
  base::DictionaryValue* attrs;
  ASSERT_TRUE(raw_attrs->GetAsDictionary(&attrs));
  ASSERT_TRUE(attrs->HasKey("cert_count"));
  ASSERT_TRUE(attrs->HasKey("cert_size"));
  ASSERT_TRUE(attrs->HasKey("undeduped_cert_size"));
  ASSERT_TRUE(attrs->HasKey("undeduped_cert_count"));
  ASSERT_TRUE(attrs->HasKey(base::trace_event::MemoryAllocatorDump::kNameSize));
}

}  // namespace net
