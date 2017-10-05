// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/chromium/http2_push_promise_index.h"

#include "base/run_loop.h"
#include "net/base/host_port_pair.h"
#include "net/base/privacy_mode.h"
#include "net/log/test_net_log.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/chromium/spdy_session.h"
#include "net/spdy/chromium/spdy_test_util_common.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {
namespace test {

class Http2PushPromiseIndexTest : public testing::Test {
 protected:
  Http2PushPromiseIndexTest()
      : url1_("https://www.example.org"),
        url2_("https://mail.example.com"),
        host_port_pair1_(HostPortPair::FromURL(url1_)),
        host_port_pair2_(HostPortPair::FromURL(url2_)),
        key1_(host_port_pair1_, ProxyServer::Direct(), PRIVACY_MODE_ENABLED),
        key2_(host_port_pair2_, ProxyServer::Direct(), PRIVACY_MODE_ENABLED),
        http_network_session_(
            SpdySessionDependencies::SpdyCreateSession(&session_deps_)) {}

  NetLogWithSource log_;
  const GURL url1_;
  const GURL url2_;
  const HostPortPair host_port_pair1_;
  const HostPortPair host_port_pair2_;
  const SpdySessionKey key1_;
  const SpdySessionKey key2_;
  SpdySessionDependencies session_deps_;
  std::unique_ptr<HttpNetworkSession> http_network_session_;
  Http2PushPromiseIndex index_;
};

TEST_F(Http2PushPromiseIndexTest, Empty) {
  EXPECT_FALSE(index_.Find(key1_, url1_));
  EXPECT_FALSE(index_.Find(key2_, url2_));
}

TEST_F(Http2PushPromiseIndexTest, FindMultipleSessionsWithDifferentUrl) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.cert = ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  // For first session.
  SequencedSocketData data1(reads, arraysize(reads), nullptr, 0);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  // For second session.
  SequencedSocketData data2(reads, arraysize(reads), nullptr, 0);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  base::WeakPtr<SpdySession> spdy_session1 =
      CreateSpdySession(http_network_session_.get(), key1_, log_);
  base::WeakPtr<SpdySession> spdy_session2 =
      CreateSpdySession(http_network_session_.get(), key2_, log_);
  // Read hanging socket data.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(index_.Find(key1_, url1_));
  EXPECT_FALSE(index_.Find(key2_, url2_));

  index_.RegisterUnclaimedPushedStream(url1_, spdy_session1);

  EXPECT_EQ(spdy_session1.get(), index_.Find(key1_, url1_).get());
  EXPECT_FALSE(index_.Find(key2_, url2_));

  index_.RegisterUnclaimedPushedStream(url2_, spdy_session2);

  EXPECT_EQ(spdy_session1.get(), index_.Find(key1_, url1_).get());
  EXPECT_EQ(spdy_session2.get(), index_.Find(key2_, url2_).get());

  index_.UnregisterUnclaimedPushedStream(url1_, spdy_session1.get());

  EXPECT_FALSE(index_.Find(key1_, url1_));
  EXPECT_EQ(spdy_session2.get(), index_.Find(key2_, url2_).get());

  index_.UnregisterUnclaimedPushedStream(url2_, spdy_session2.get());

  EXPECT_FALSE(index_.Find(key1_, url1_));
  EXPECT_FALSE(index_.Find(key2_, url2_));

  // SpdySession weak pointers must still be valid,
  // otherwise comparisons above are not meaningful.
  EXPECT_TRUE(spdy_session1);
  EXPECT_TRUE(spdy_session2);

  EXPECT_TRUE(data1.AllReadDataConsumed());
  EXPECT_TRUE(data1.AllWriteDataConsumed());
  EXPECT_TRUE(data2.AllReadDataConsumed());
  EXPECT_TRUE(data2.AllWriteDataConsumed());
}

TEST_F(Http2PushPromiseIndexTest, MultipleSessionsForSingleUrl) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_IO_PENDING, 0)};
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.cert = ImportCertFromFile(GetTestCertsDirectory(), "spdy_pooling.pem");
  // For first session.
  SequencedSocketData data1(reads, arraysize(reads), nullptr, 0);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(&data1);
  // For second session.
  SequencedSocketData data2(reads, arraysize(reads), nullptr, 0);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.socket_factory->AddSocketDataProvider(&data2);

  base::WeakPtr<SpdySession> spdy_session1 =
      CreateSpdySession(http_network_session_.get(), key1_, log_);
  base::WeakPtr<SpdySession> spdy_session2 =
      CreateSpdySession(http_network_session_.get(), key2_, log_);
  // Read hanging socket data.
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(index_.Find(key1_, url1_));
  EXPECT_FALSE(index_.Find(key2_, url1_));
  EXPECT_FALSE(index_.Find(key1_, url2_));
  EXPECT_FALSE(index_.Find(key2_, url2_));

  index_.RegisterUnclaimedPushedStream(url1_, spdy_session1);

  // Note that Find() only uses its SpdySessionKey argument to verify proxy and
  // privacy mode.  Cross-origin pooling is supported, therefore HostPortPair of
  // SpdySessionKey does not matter.
  EXPECT_EQ(spdy_session1.get(), index_.Find(key1_, url1_).get());
  EXPECT_EQ(spdy_session1.get(), index_.Find(key2_, url1_).get());
  EXPECT_FALSE(index_.Find(key1_, url2_));
  EXPECT_FALSE(index_.Find(key2_, url2_));

  index_.RegisterUnclaimedPushedStream(url1_, spdy_session2);

  // Find returns the first SpdySession if there are multiple for the same URL.
  EXPECT_EQ(spdy_session1.get(), index_.Find(key1_, url1_).get());
  EXPECT_EQ(spdy_session1.get(), index_.Find(key2_, url1_).get());
  EXPECT_FALSE(index_.Find(key1_, url2_));
  EXPECT_FALSE(index_.Find(key2_, url2_));

  index_.UnregisterUnclaimedPushedStream(url1_, spdy_session1.get());

  EXPECT_EQ(spdy_session2.get(), index_.Find(key1_, url1_).get());
  EXPECT_EQ(spdy_session2.get(), index_.Find(key2_, url1_).get());
  EXPECT_FALSE(index_.Find(key1_, url2_));
  EXPECT_FALSE(index_.Find(key2_, url2_));

  index_.UnregisterUnclaimedPushedStream(url1_, spdy_session2.get());

  EXPECT_FALSE(index_.Find(key1_, url1_));
  EXPECT_FALSE(index_.Find(key2_, url1_));
  EXPECT_FALSE(index_.Find(key1_, url2_));
  EXPECT_FALSE(index_.Find(key2_, url2_));

  // SpdySession weak pointers must still be valid,
  // otherwise comparisons above are not meaningful.
  EXPECT_TRUE(spdy_session1);
  EXPECT_TRUE(spdy_session2);

  EXPECT_TRUE(data1.AllReadDataConsumed());
  EXPECT_TRUE(data1.AllWriteDataConsumed());
  EXPECT_TRUE(data2.AllReadDataConsumed());
  EXPECT_TRUE(data2.AllWriteDataConsumed());
}

}  // namespace test
}  // namespace net
