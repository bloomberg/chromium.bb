// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/chromium/http2_push_promise_index.h"

#include "net/base/host_port_pair.h"
#include "net/base/privacy_mode.h"
#include "net/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// For simplicity, these tests do not create SpdySession instances
// (necessary for a non-null WeakPtr<SpdySession>), instead they use nullptr.
// Streams are identified by SpdyStreamId only.

using ::testing::Return;
using ::testing::_;

namespace net {
namespace test {
namespace {

// Delegate implementation for tests that requires exact match of SpdySessionKey
// in ValidatePushedStream().  Note that SpdySession, unlike TestDelegate,
// allows cross-origin pooling.
class TestDelegate : public Http2PushPromiseIndex::Delegate {
 public:
  TestDelegate() = delete;
  TestDelegate(const SpdySessionKey& key) : key_(key) {}
  ~TestDelegate() override {}

  bool ValidatePushedStream(const SpdySessionKey& key) const override {
    return key == key_;
  }

  void OnPushedStreamClaimed(const GURL& url, SpdyStreamId stream_id) override {
  }

  base::WeakPtr<SpdySession> GetWeakPtrToSession() override { return nullptr; }

 private:
  SpdySessionKey key_;
};

// Mock implementation.
class MockDelegate : public Http2PushPromiseIndex::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override {}

  MOCK_CONST_METHOD1(ValidatePushedStream, bool(const SpdySessionKey& key));
  MOCK_METHOD2(OnPushedStreamClaimed,
               void(const GURL& url, SpdyStreamId stream_id));

  base::WeakPtr<SpdySession> GetWeakPtrToSession() override { return nullptr; }
};

}  // namespace

class Http2PushPromiseIndexPeer {
 public:
  using UnclaimedPushedStream = Http2PushPromiseIndex::UnclaimedPushedStream;
  using CompareByUrl = Http2PushPromiseIndex::CompareByUrl;
};

class Http2PushPromiseIndexTest : public testing::Test {
 protected:
  Http2PushPromiseIndexTest()
      : url1_("https://www.example.org"),
        url2_("https://mail.example.com"),
        key1_(HostPortPair::FromURL(url1_),
              ProxyServer::Direct(),
              PRIVACY_MODE_ENABLED),
        key2_(HostPortPair::FromURL(url2_),
              ProxyServer::Direct(),
              PRIVACY_MODE_ENABLED) {}

  const GURL url1_;
  const GURL url2_;
  const SpdySessionKey key1_;
  const SpdySessionKey key2_;
  Http2PushPromiseIndex index_;
};

// If |index_| is empty, then FindSession() should set its |stream_id| outparam
// to kNoPushedStreamFound for any values of inparams.
TEST_F(Http2PushPromiseIndexTest, Empty) {
  base::WeakPtr<SpdySession> session;
  SpdyStreamId stream_id = 2;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.FindSession(key1_, url2_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.FindSession(key1_, url2_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.FindSession(key2_, url2_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);
}

// Trying to unregister a stream not in the index should log to DFATAL.
// Case 1: no streams for the given URL.
TEST_F(Http2PushPromiseIndexTest, UnregisterNonexistingEntryCrashes1) {
  TestDelegate delegate(key1_);
  EXPECT_DFATAL(index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate),
                "Only a previously registered entry can be unregistered.");
}

// Trying to unregister a stream not in the index should log to DFATAL.
// Case 2: there is a stream for the given URL, but not with the same stream ID.
TEST_F(Http2PushPromiseIndexTest, UnregisterNonexistingEntryCrashes2) {
  TestDelegate delegate(key1_);
  index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate);
  EXPECT_DFATAL(index_.UnregisterUnclaimedPushedStream(url1_, 4, &delegate),
                "Only a previously registered entry can be unregistered.");
  // Stream must be unregistered so that Http2PushPromiseIndex destructor
  // does not crash.
  index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate);
}

// Create two entries, both with a delegate that requires |key| to be equal to
// |key1_|.  Register the two entries with different URLs.  Check that they can
// be found by their respective URLs.
TEST_F(Http2PushPromiseIndexTest, FindMultipleStreamsWithDifferentUrl) {
  // Register first entry.
  TestDelegate delegate1(key1_);
  index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1);

  // Retrieve first entry by its URL, no entry found for |url2_|.
  base::WeakPtr<SpdySession> session;
  SpdyStreamId stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(2u, stream_id);

  stream_id = 2;
  index_.FindSession(key1_, url2_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  // Register second entry.
  TestDelegate delegate2(key1_);
  index_.RegisterUnclaimedPushedStream(url2_, 4, &delegate2);

  // Retrieve each entry by their respective URLs.
  stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(2u, stream_id);

  stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url2_, &session, &stream_id);
  EXPECT_EQ(4u, stream_id);

  // Unregister first entry.
  index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1);

  // No entry found for |url1_|, retrieve second entry by its URL.
  stream_id = 2;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url2_, &session, &stream_id);
  EXPECT_EQ(4u, stream_id);

  // Unregister second entry.
  index_.UnregisterUnclaimedPushedStream(url2_, 4, &delegate2);

  // No entries found for either URL.
  stream_id = 2;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.FindSession(key1_, url2_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);
}

// Create two entries with delegates that validate different SpdySessionKeys.
// Register the two entries with the same URL.  Check that they can be found by
// their respective SpdySessionKeys.
TEST_F(Http2PushPromiseIndexTest, MultipleStreamsWithDifferentKeys) {
  // Register first entry.
  TestDelegate delegate1(key1_);
  index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1);

  // Retrieve first entry by its SpdySessionKey, no entry found for |key2_|.
  base::WeakPtr<SpdySession> session;
  SpdyStreamId stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(2u, stream_id);

  stream_id = 2;
  index_.FindSession(key2_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  // Register second entry.
  TestDelegate delegate2(key2_);
  index_.RegisterUnclaimedPushedStream(url1_, 4, &delegate2);

  // Retrieve each entry by their respective SpdySessionKeys.
  stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(2u, stream_id);

  stream_id = kNoPushedStreamFound;
  index_.FindSession(key2_, url1_, &session, &stream_id);
  EXPECT_EQ(4u, stream_id);

  // Unregister first entry.
  index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1);

  // No entry found for |key1_|, retrieve second entry by its SpdySessionKey.
  stream_id = 2;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = kNoPushedStreamFound;
  index_.FindSession(key2_, url1_, &session, &stream_id);
  EXPECT_EQ(4u, stream_id);

  // Unregister second entry.
  index_.UnregisterUnclaimedPushedStream(url1_, 4, &delegate2);

  // No entries found for either SpdySessionKeys.
  stream_id = 2;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  stream_id = 2;
  index_.FindSession(key2_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);
}

TEST_F(Http2PushPromiseIndexTest, MultipleMatchingStreams) {
  // Register two entries with identical URLs that have delegates that accept
  // the same SpdySessionKey.
  TestDelegate delegate1(key1_);
  TestDelegate delegate2(key1_);
  index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate1);
  index_.RegisterUnclaimedPushedStream(url1_, 4, &delegate2);

  // Test that FindSession() returns one of the two entries.  FindSession()
  // makes no guarantee about which entry it returns if there are multiple
  // matches.
  base::WeakPtr<SpdySession> session;
  SpdyStreamId stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_NE(kNoPushedStreamFound, stream_id);

  // Unregister the first entry.
  index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate1);

  // Test that the second entry can still be retrieved.
  stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(4u, stream_id);

  // Unregister the second entry.
  index_.UnregisterUnclaimedPushedStream(url1_, 4, &delegate2);

  // Test that no entry is found.
  stream_id = 2;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);
}

// Test that Delegate::ValidatePushedStream() is called by FindSession(), and if
// it returns true, then Delegate::OnPushedStreamClaimed() is called with the
// appropriate arguments.
TEST_F(Http2PushPromiseIndexTest, MatchCallsOnPushedStreamClaimed) {
  MockDelegate delegate;
  EXPECT_CALL(delegate, ValidatePushedStream(key1_)).WillOnce(Return(true));
  EXPECT_CALL(delegate, OnPushedStreamClaimed(url1_, 2)).Times(1);

  index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate);

  base::WeakPtr<SpdySession> session;
  SpdyStreamId stream_id = kNoPushedStreamFound;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(2u, stream_id);

  index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate);
};

// Test that Delegate::ValidatePushedStream() is called by FindSession(), and if
// it returns false, then Delegate::OnPushedStreamClaimed() is not called.
TEST_F(Http2PushPromiseIndexTest, MismatchDoesNotCallOnPushedStreamClaimed) {
  MockDelegate delegate;
  EXPECT_CALL(delegate, ValidatePushedStream(key1_)).WillOnce(Return(false));
  EXPECT_CALL(delegate, OnPushedStreamClaimed(_, _)).Times(0);

  index_.RegisterUnclaimedPushedStream(url1_, 2, &delegate);

  base::WeakPtr<SpdySession> session;
  SpdyStreamId stream_id = 2;
  index_.FindSession(key1_, url1_, &session, &stream_id);
  EXPECT_EQ(kNoPushedStreamFound, stream_id);

  index_.UnregisterUnclaimedPushedStream(url1_, 2, &delegate);
};

// Test that an entry is equivalent to itself.
TEST(Http2PushPromiseIndexCompareByUrlTest, Reflexivity) {
  // Test with two entries: with and without a pushed stream.
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry1{GURL(), 2, nullptr};
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry2{
      GURL(), kNoPushedStreamFound, nullptr};

  // For "Compare", it is a requirement that comp(A, A) == false, see
  // http://en.cppreference.com/w/cpp/concept/Compare.  This will in fact imply
  // that equiv(A, A) == true.
  EXPECT_FALSE(Http2PushPromiseIndexPeer::CompareByUrl()(entry1, entry1));
  EXPECT_FALSE(Http2PushPromiseIndexPeer::CompareByUrl()(entry2, entry2));

  std::set<Http2PushPromiseIndexPeer::UnclaimedPushedStream,
           Http2PushPromiseIndexPeer::CompareByUrl>
      entries;
  bool success;
  std::tie(std::ignore, success) = entries.insert(entry1);
  EXPECT_TRUE(success);

  // Test that |entry1| is considered equivalent to itself by ensuring that
  // a second insertion fails.
  std::tie(std::ignore, success) = entries.insert(entry1);
  EXPECT_FALSE(success);

  // Test that |entry1| and |entry2| are not equivalent.
  std::tie(std::ignore, success) = entries.insert(entry2);
  EXPECT_TRUE(success);

  // Test that |entry2| is equivalent to an existing entry
  // (which then must be |entry2|).
  std::tie(std::ignore, success) = entries.insert(entry2);
  EXPECT_FALSE(success);
};

TEST(Http2PushPromiseIndexCompareByUrlTest, LookupByURL) {
  const GURL url1("https://example.com:1");
  const GURL url2("https://example.com:2");
  const GURL url3("https://example.com:3");
  // This test relies on the order of these GURLs.
  ASSERT_LT(url1, url2);
  ASSERT_LT(url2, url3);

  // Create four entries, two for the middle URL, with distinct stream IDs not
  // in ascending order.
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry1{url1, 8, nullptr};
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry2{url2, 4, nullptr};
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry3{url2, 6, nullptr};
  Http2PushPromiseIndexPeer::UnclaimedPushedStream entry4{url3, 2, nullptr};

  // Fill up a set.
  std::set<Http2PushPromiseIndexPeer::UnclaimedPushedStream,
           Http2PushPromiseIndexPeer::CompareByUrl>
      entries;
  entries.insert(entry1);
  entries.insert(entry2);
  entries.insert(entry3);
  entries.insert(entry4);
  ASSERT_EQ(4u, entries.size());

  // Test that entries are ordered by URL first, not stream ID.
  std::set<Http2PushPromiseIndexPeer::UnclaimedPushedStream,
           Http2PushPromiseIndexPeer::CompareByUrl>::iterator it =
      entries.begin();
  EXPECT_EQ(8u, it->stream_id);
  ++it;
  EXPECT_EQ(4u, it->stream_id);
  ++it;
  EXPECT_EQ(6u, it->stream_id);
  ++it;
  EXPECT_EQ(2u, it->stream_id);
  ++it;
  EXPECT_TRUE(it == entries.end());

  // Test that kNoPushedStreamFound can be used to look up the first entry for a
  // given URL.  In particular, the first entry with |url2| is |entry2|.
  EXPECT_TRUE(
      entries.lower_bound(Http2PushPromiseIndexPeer::UnclaimedPushedStream{
          url2, kNoPushedStreamFound, nullptr}) == entries.find(entry2));
};

}  // namespace test
}  // namespace net
