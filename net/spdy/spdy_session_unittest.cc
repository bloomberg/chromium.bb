// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_log_unittest.h"
#include "net/base/request_priority.h"
#include "net/base/test_data_directory.h"
#include "net/base/test_data_stream.h"
#include "net/dns/host_cache.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/spdy/spdy_session_test_util.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/spdy/spdy_test_utils.h"
#include "net/test/cert_test_util.h"
#include "testing/platform_test.h"

namespace net {

namespace {

static const char kTestUrl[] = "http://www.example.org/";
static const char kTestHost[] = "www.example.org";
static const int kTestPort = 80;

const char kBodyData[] = "Body data";
const size_t kBodyDataSize = arraysize(kBodyData);
const base::StringPiece kBodyDataStringPiece(kBodyData, kBodyDataSize);

static int g_delta_seconds = 0;
base::TimeTicks TheNearFuture() {
  return base::TimeTicks::Now() + base::TimeDelta::FromSeconds(g_delta_seconds);
}

}  // namespace

class SpdySessionTest : public PlatformTest,
                        public ::testing::WithParamInterface<NextProto> {
 public:
  // Functions used with RunResumeAfterUnstallTest().

  void StallSessionOnly(SpdySession* session, SpdyStream* stream) {
    StallSessionSend(session);
  }

  void StallStreamOnly(SpdySession* session, SpdyStream* stream) {
    StallStreamSend(stream);
  }

  void StallSessionStream(SpdySession* session, SpdyStream* stream) {
    StallSessionSend(session);
    StallStreamSend(stream);
  }

  void StallStreamSession(SpdySession* session, SpdyStream* stream) {
    StallStreamSend(stream);
    StallSessionSend(session);
  }

  void UnstallSessionOnly(SpdySession* session,
                          SpdyStream* stream,
                          int32 delta_window_size) {
    UnstallSessionSend(session, delta_window_size);
  }

  void UnstallStreamOnly(SpdySession* session,
                         SpdyStream* stream,
                         int32 delta_window_size) {
    UnstallStreamSend(stream, delta_window_size);
  }

  void UnstallSessionStream(SpdySession* session,
                            SpdyStream* stream,
                            int32 delta_window_size) {
    UnstallSessionSend(session, delta_window_size);
    UnstallStreamSend(stream, delta_window_size);
  }

  void UnstallStreamSession(SpdySession* session,
                            SpdyStream* stream,
                            int32 delta_window_size) {
    UnstallStreamSend(stream, delta_window_size);
    UnstallSessionSend(session, delta_window_size);
  }

 protected:
  SpdySessionTest()
      : old_max_group_sockets_(ClientSocketPoolManager::max_sockets_per_group(
            HttpNetworkSession::NORMAL_SOCKET_POOL)),
        old_max_pool_sockets_(ClientSocketPoolManager::max_sockets_per_pool(
            HttpNetworkSession::NORMAL_SOCKET_POOL)),
        spdy_util_(GetParam()),
        session_deps_(GetParam()),
        spdy_session_pool_(NULL),
        test_url_(kTestUrl),
        test_host_port_pair_(kTestHost, kTestPort),
        key_(test_host_port_pair_, ProxyServer::Direct(),
             kPrivacyModeDisabled) {
  }

  virtual ~SpdySessionTest() {
    // Important to restore the per-pool limit first, since the pool limit must
    // always be greater than group limit, and the tests reduce both limits.
    ClientSocketPoolManager::set_max_sockets_per_pool(
        HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_pool_sockets_);
    ClientSocketPoolManager::set_max_sockets_per_group(
        HttpNetworkSession::NORMAL_SOCKET_POOL, old_max_group_sockets_);
  }

  virtual void SetUp() OVERRIDE {
    g_delta_seconds = 0;
  }

  void CreateDeterministicNetworkSession() {
    http_session_ =
        SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);
    spdy_session_pool_ = http_session_->spdy_session_pool();
  }

  void CreateNetworkSession() {
    http_session_ =
        SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    spdy_session_pool_ = http_session_->spdy_session_pool();
  }

  scoped_refptr<SpdySession> GetSession(const SpdySessionKey& key) {
    EXPECT_FALSE(spdy_session_pool_->HasSession(key));
    scoped_refptr<SpdySession> session =
        spdy_session_pool_->Get(key, BoundNetLog());
    EXPECT_TRUE(spdy_session_pool_->HasSession(key));
    return session;
  }

  // Creates an initialized session to |key_|.
  scoped_refptr<SpdySession> CreateInitializedSession() {
    scoped_refptr<SpdySession> session = GetSession(key_);
    EXPECT_EQ(
        OK,
        InitializeSession(
            http_session_.get(), session.get(), test_host_port_pair_));
    return session;
  }

  net::Error InitializeSession(HttpNetworkSession* http_session,
                               SpdySession* session,
                               const HostPortPair& host_port_pair) {
    transport_params_ = new TransportSocketParams(
        host_port_pair, MEDIUM, false, false, OnHostResolutionCallback());

    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    EXPECT_EQ(OK, connection->Init(host_port_pair.ToString(),
                                   transport_params_, MEDIUM,
                                   CompletionCallback(),
                                   http_session->GetTransportSocketPool(
                                       HttpNetworkSession::NORMAL_SOCKET_POOL),
                                   BoundNetLog()));
    return session->InitializeWithSocket(connection.release(), false, OK);
  }

  void StallSessionSend(SpdySession* session) {
    // Reduce the send window size to 0 to stall.
    while (session->session_send_window_size_ > 0) {
      session->DecreaseSendWindowSize(
          std::min(kMaxSpdyFrameChunkSize, session->session_send_window_size_));
    }
  }

  void UnstallSessionSend(SpdySession* session, int32 delta_window_size) {
    session->IncreaseSendWindowSize(delta_window_size);
  }

  void StallStreamSend(SpdyStream* stream) {
    // Reduce the send window size to 0 to stall.
    while (stream->send_window_size() > 0) {
      stream->DecreaseSendWindowSize(
          std::min(kMaxSpdyFrameChunkSize, stream->send_window_size()));
    }
  }

  void UnstallStreamSend(SpdyStream* stream, int32 delta_window_size) {
    stream->IncreaseSendWindowSize(delta_window_size);
  }

  void RunResumeAfterUnstallTest(
      const base::Callback<void(SpdySession*, SpdyStream*)>& stall_fn,
      const base::Callback<void(SpdySession*, SpdyStream*, int32)>&
          unstall_function);

  // Original socket limits.  Some tests set these.  Safest to always restore
  // them once each test has been run.
  int old_max_group_sockets_;
  int old_max_pool_sockets_;

  SpdyTestUtil spdy_util_;
  scoped_refptr<TransportSocketParams> transport_params_;
  SpdySessionDependencies session_deps_;
  scoped_refptr<HttpNetworkSession> http_session_;
  SpdySessionPool* spdy_session_pool_;
  GURL test_url_;
  HostPortPair test_host_port_pair_;
  SpdySessionKey key_;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    SpdySessionTest,
    testing::Values(kProtoSPDY2, kProtoSPDY3, kProtoSPDY31, kProtoSPDY4a2));

// TODO(akalin): Don't early-exit in the tests below for values >
// kProtoSPDY3.

TEST_P(SpdySessionTest, GoAway) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> goaway(spdy_util_.ConstructSpdyGoAway(1));
  MockRead reads[] = {
    CreateMockRead(*goaway, 2),
    MockRead(ASYNC, 0, 3)  // EOF
  };
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, MEDIUM, true));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, MEDIUM, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
    CreateMockWrite(*req2, 1),
  };
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  EXPECT_EQ(spdy_util_.spdy_version(), session->GetProtocolVersion());

  GURL url("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, MEDIUM, BoundNetLog());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, MEDIUM, BoundNetLog());
  test::StreamDelegateDoNothing delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(url.spec()));
  scoped_ptr<SpdyHeaderBlock> headers2(new SpdyHeaderBlock(*headers));

  spdy_stream1->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());
  spdy_stream2->SendRequestHeaders(headers2.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream2->HasUrl());

  data.RunFor(2);

  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  EXPECT_TRUE(spdy_session_pool_->HasSession(key_));

  // Read and process the GOAWAY frame.
  data.RunFor(1);

  EXPECT_FALSE(spdy_session_pool_->HasSession(key_));

  EXPECT_TRUE(session->IsStreamActive(1));
  EXPECT_FALSE(session->IsStreamActive(3));

  scoped_refptr<SpdySession> session2 = GetSession(key_);

  spdy_stream1->Close();
  EXPECT_EQ(NULL, spdy_stream1.get());

  // Delete the first session.
  session = NULL;

  // Delete the second session.
  spdy_session_pool_->Remove(session2);
  session2 = NULL;
  EXPECT_EQ(NULL, spdy_stream2.get());
}

TEST_P(SpdySessionTest, ClientPing) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.enable_ping = true;
  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> read_ping(spdy_util_.ConstructSpdyPing(1));
  MockRead reads[] = {
    CreateMockRead(*read_ping),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  scoped_ptr<SpdyFrame> write_ping(spdy_util_.ConstructSpdyPing(1));
  MockWrite writes[] = {
    CreateMockWrite(*write_ping),
  };
  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, test_url_, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  test::StreamDelegateSendImmediate delegate(spdy_stream1, NULL);
  spdy_stream1->SetDelegate(&delegate);

  base::TimeTicks before_ping_time = base::TimeTicks::Now();

  session->set_connection_at_risk_of_loss_time(
      base::TimeDelta::FromSeconds(-1));
  session->set_hung_interval(base::TimeDelta::FromMilliseconds(50));

  session->SendPrefacePingIfNoneInFlight();

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  session->CheckPingStatus(before_ping_time);

  EXPECT_EQ(0, session->pings_in_flight());
  EXPECT_GE(session->next_ping_id(), static_cast<uint32>(1));
  EXPECT_FALSE(session->check_ping_status_pending());
  EXPECT_GE(session->last_activity_time(), before_ping_time);

  EXPECT_FALSE(spdy_session_pool_->HasSession(key_));

  // Delete the first session.
  session = NULL;
}

TEST_P(SpdySessionTest, ServerPing) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> read_ping(spdy_util_.ConstructSpdyPing(2));
  MockRead reads[] = {
    CreateMockRead(*read_ping),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  scoped_ptr<SpdyFrame> write_ping(spdy_util_.ConstructSpdyPing(2));
  MockWrite writes[] = {
    CreateMockWrite(*write_ping),
  };
  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, test_url_, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  test::StreamDelegateSendImmediate delegate(spdy_stream1, NULL);
  spdy_stream1->SetDelegate(&delegate);

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(spdy_session_pool_->HasSession(key_));

  // Delete the session.
  session = NULL;
  EXPECT_EQ(NULL, spdy_stream1.get());
}

TEST_P(SpdySessionTest, DeleteExpiredPushStreams) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);
  session_deps_.time_func = TheNearFuture;

  CreateNetworkSession();

  scoped_refptr<SpdySession> session = GetSession(key_);

  session->buffered_spdy_framer_.reset(
      new BufferedSpdyFramer(spdy_util_.spdy_version(), false));

  // Create the associated stream and add to active streams.
  scoped_ptr<SpdyHeaderBlock> request_headers(
      spdy_util_.ConstructGetHeaderBlock("http://www.google.com/"));

  scoped_ptr<SpdyStream> stream(new SpdyStream(SPDY_REQUEST_RESPONSE_STREAM,
                                               session.get(),
                                               std::string(),
                                               DEFAULT_PRIORITY,
                                               kSpdyStreamInitialWindowSize,
                                               kSpdyStreamInitialWindowSize,
                                               session->net_log_));
  stream->SendRequestHeaders(request_headers.Pass(), NO_MORE_DATA_TO_SEND);
  SpdyStream* stream_ptr = stream.get();
  session->InsertCreatedStream(stream.Pass());
  stream = session->ActivateCreatedStream(stream_ptr);
  session->InsertActivatedStream(stream.Pass());

  SpdyHeaderBlock headers;
  spdy_util_.AddUrlToHeaderBlock("http://www.google.com/a.dat", &headers);
  session->OnSynStream(2, 1, 0, 0, true, false, headers);

  // Verify that there is one unclaimed push stream.
  EXPECT_EQ(1u, session->num_unclaimed_pushed_streams());
  SpdySession::PushedStreamMap::iterator iter =
      session->unclaimed_pushed_streams_.find("http://www.google.com/a.dat");
  EXPECT_TRUE(session->unclaimed_pushed_streams_.end() != iter);

  // Shift time.
  g_delta_seconds = 301;

  spdy_util_.AddUrlToHeaderBlock("http://www.google.com/b.dat", &headers);
  session->OnSynStream(4, 1, 0, 0, true, false, headers);

  // Verify that the second pushed stream evicted the first pushed stream.
  EXPECT_EQ(1u, session->num_unclaimed_pushed_streams());
  iter = session->unclaimed_pushed_streams_.find("http://www.google.com/b.dat");
  EXPECT_TRUE(session->unclaimed_pushed_streams_.end() != iter);

  // Delete the session.
  session = NULL;
}

TEST_P(SpdySessionTest, FailedPing) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> read_ping(spdy_util_.ConstructSpdyPing(1));
  MockRead reads[] = {
    CreateMockRead(*read_ping),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  scoped_ptr<SpdyFrame> write_ping(spdy_util_.ConstructSpdyPing(1));
  MockWrite writes[] = {
    CreateMockWrite(*write_ping),
  };
  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, test_url_, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  test::StreamDelegateSendImmediate delegate(spdy_stream1, NULL);
  spdy_stream1->SetDelegate(&delegate);

  session->set_connection_at_risk_of_loss_time(base::TimeDelta::FromSeconds(0));
  session->set_hung_interval(base::TimeDelta::FromSeconds(0));

  // Send a PING frame.
  session->WritePingFrame(1);
  EXPECT_LT(0, session->pings_in_flight());
  EXPECT_GE(session->next_ping_id(), static_cast<uint32>(1));
  EXPECT_TRUE(session->check_ping_status_pending());

  // Assert session is not closed.
  EXPECT_FALSE(session->IsClosed());
  EXPECT_LT(0u, session->num_active_streams() + session->num_created_streams());
  EXPECT_TRUE(spdy_session_pool_->HasSession(key_));

  // We set last time we have received any data in 1 sec less than now.
  // CheckPingStatus will trigger timeout because hung interval is zero.
  base::TimeTicks now = base::TimeTicks::Now();
  session->last_activity_time_ = now - base::TimeDelta::FromSeconds(1);
  session->CheckPingStatus(now);

  EXPECT_TRUE(session->IsClosed());
  EXPECT_EQ(0u, session->num_active_streams());
  EXPECT_EQ(0u, session->num_unclaimed_pushed_streams());
  EXPECT_FALSE(spdy_session_pool_->HasSession(key_));

  // Delete the first session.
  session = NULL;
  EXPECT_EQ(NULL, spdy_stream1.get());
}

TEST_P(SpdySessionTest, CloseIdleSessions) {
  if (GetParam() > kProtoSPDY3)
    return;

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();

  // Set up session 1
  const std::string kTestHost1("http://www.a.com");
  HostPortPair test_host_port_pair1(kTestHost1, 80);
  SpdySessionKey key1(test_host_port_pair1, ProxyServer::Direct(),
                      kPrivacyModeDisabled);
  scoped_refptr<SpdySession> session1 = GetSession(key1);
  EXPECT_EQ(
      OK,
      InitializeSession(
          http_session_.get(), session1.get(), test_host_port_pair1));
  GURL url1(kTestHost1);
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session1, url1, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);

  // Set up session 2
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  const std::string kTestHost2("http://www.b.com");
  HostPortPair test_host_port_pair2(kTestHost2, 80);
  SpdySessionKey key2(test_host_port_pair2, ProxyServer::Direct(),
                      kPrivacyModeDisabled);
  scoped_refptr<SpdySession> session2 = GetSession(key2);
  EXPECT_EQ(
      OK,
      InitializeSession(
          http_session_.get(), session2.get(), test_host_port_pair2));
  GURL url2(kTestHost2);
  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session2, url2, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream2.get() != NULL);

  // Set up session 3
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  const std::string kTestHost3("http://www.c.com");
  HostPortPair test_host_port_pair3(kTestHost3, 80);
  SpdySessionKey key3(test_host_port_pair3, ProxyServer::Direct(),
                      kPrivacyModeDisabled);
  scoped_refptr<SpdySession> session3 = GetSession(key3);
  EXPECT_EQ(
      OK,
      InitializeSession(
          http_session_.get(), session3.get(), test_host_port_pair3));
  GURL url3(kTestHost3);
  base::WeakPtr<SpdyStream> spdy_stream3 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session3, url3, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream3.get() != NULL);

  // All sessions are active and not closed
  EXPECT_TRUE(session1->is_active());
  EXPECT_FALSE(session1->IsClosed());
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());
  EXPECT_TRUE(session3->is_active());
  EXPECT_FALSE(session3->IsClosed());

  // Should not do anything, all are active
  spdy_session_pool_->CloseIdleSessions();
  EXPECT_TRUE(session1->is_active());
  EXPECT_FALSE(session1->IsClosed());
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());
  EXPECT_TRUE(session3->is_active());
  EXPECT_FALSE(session3->IsClosed());

  // Make sessions 1 and 3 inactive, but keep them open.
  // Session 2 still open and active
  session1->CloseCreatedStream(spdy_stream1, OK);
  EXPECT_EQ(NULL, spdy_stream1.get());
  session3->CloseCreatedStream(spdy_stream3, OK);
  EXPECT_EQ(NULL, spdy_stream3.get());
  EXPECT_FALSE(session1->is_active());
  EXPECT_FALSE(session1->IsClosed());
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());
  EXPECT_FALSE(session3->is_active());
  EXPECT_FALSE(session3->IsClosed());

  // Should close session 1 and 3, 2 should be left open
  spdy_session_pool_->CloseIdleSessions();
  EXPECT_FALSE(session1->is_active());
  EXPECT_TRUE(session1->IsClosed());
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());
  EXPECT_FALSE(session3->is_active());
  EXPECT_TRUE(session3->IsClosed());

  // Should not do anything
  spdy_session_pool_->CloseIdleSessions();
  EXPECT_TRUE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());

  // Make 2 not active
  session2->CloseCreatedStream(spdy_stream2, OK);
  EXPECT_EQ(NULL, spdy_stream2.get());
  EXPECT_FALSE(session2->is_active());
  EXPECT_FALSE(session2->IsClosed());

  // This should close session 2
  spdy_session_pool_->CloseIdleSessions();
  EXPECT_FALSE(session2->is_active());
  EXPECT_TRUE(session2->IsClosed());
}

// Start with max concurrent streams set to 1.  Request two streams.  Receive a
// settings frame setting max concurrent streams to 2.  Have the callback
// release the stream, which releases its reference (the last) to the session.
// Make sure nothing blows up.
// http://crbug.com/57331
TEST_P(SpdySessionTest, OnSettings) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  SettingsMap new_settings;
  const SpdySettingsIds kSpdySettingsIds1 = SETTINGS_MAX_CONCURRENT_STREAMS;
  const uint32 max_concurrent_streams = 2;
  new_settings[kSpdySettingsIds1] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, max_concurrent_streams);

  // Set up the socket so we read a SETTINGS frame that raises max concurrent
  // streams to 2.
  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(new_settings));
  MockRead reads[] = {
    CreateMockRead(*settings_frame),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Initialize the SpdySetting with 1 max concurrent streams.
  spdy_session_pool_->http_server_properties()->SetSpdySetting(
      test_host_port_pair_,
      kSpdySettingsIds1,
      SETTINGS_FLAG_PLEASE_PERSIST,
      1);

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  // Create 2 streams.  First will succeed.  Second will be pending.
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, test_url_, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);

  StreamReleaserCallback stream_releaser;
  SpdyStreamRequest request;
  ASSERT_EQ(ERR_IO_PENDING,
            request.StartRequest(
                SPDY_BIDIRECTIONAL_STREAM, session, test_url_, MEDIUM,
                BoundNetLog(),
                stream_releaser.MakeCallback(&request)));
  session = NULL;

  EXPECT_EQ(OK, stream_releaser.WaitForResult());
}

// Start with max concurrent streams set to 1 (that is persisted). Receive a
// settings frame setting max concurrent streams to 2 and which also clears the
// persisted data. Verify that persisted data is correct.
TEST_P(SpdySessionTest, ClearSettings) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  SettingsMap new_settings;
  const SpdySettingsIds kSpdySettingsIds1 = SETTINGS_MAX_CONCURRENT_STREAMS;
  const uint32 max_concurrent_streams = 2;
  new_settings[kSpdySettingsIds1] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, max_concurrent_streams);

  // Set up the socket so we read a SETTINGS frame that raises max concurrent
  // streams to 2 and clears previously persisted data.
  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(new_settings));
  uint8 flags = SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS;
  test::SetFrameFlags(settings_frame.get(), flags, SPDY3);
  MockRead reads[] = {
    CreateMockRead(*settings_frame),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Initialize the SpdySetting with 1 max concurrent streams.
  spdy_session_pool_->http_server_properties()->SetSpdySetting(
      test_host_port_pair_,
      kSpdySettingsIds1,
      SETTINGS_FLAG_PLEASE_PERSIST,
      1);

  EXPECT_EQ(1u, spdy_session_pool_->http_server_properties()->GetSpdySettings(
      test_host_port_pair_).size());

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  // Create 2 streams.  First will succeed.  Second will be pending.
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, test_url_, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);

  StreamReleaserCallback stream_releaser;

  SpdyStreamRequest request;
  ASSERT_EQ(ERR_IO_PENDING,
            request.StartRequest(
                SPDY_BIDIRECTIONAL_STREAM, session, test_url_, MEDIUM,
                BoundNetLog(),
                stream_releaser.MakeCallback(&request)));

  EXPECT_EQ(OK, stream_releaser.WaitForResult());

  // Make sure that persisted data is cleared.
  EXPECT_EQ(0u, spdy_session_pool_->http_server_properties()->GetSpdySettings(
      test_host_port_pair_).size());

  // Make sure session's max_concurrent_streams is 2.
  EXPECT_EQ(2u, session->max_concurrent_streams());

  session = NULL;
}

// Start with max concurrent streams set to 1.  Request two streams.  When the
// first completes, have the callback close itself, which should trigger the
// second stream creation.  Then cancel that one immediately.  Don't crash.
// http://crbug.com/63532
TEST_P(SpdySessionTest, CancelPendingCreateStream) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  MockConnect connect_data(SYNCHRONOUS, OK);

  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Initialize the SpdySetting with 1 max concurrent streams.
  spdy_session_pool_->http_server_properties()->SetSpdySetting(
      test_host_port_pair_,
      SETTINGS_MAX_CONCURRENT_STREAMS,
      SETTINGS_FLAG_PLEASE_PERSIST,
      1);

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  // Create 2 streams.  First will succeed.  Second will be pending.
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, test_url_, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);

  // Use scoped_ptr to let us invalidate the memory when we want to, to trigger
  // a valgrind error if the callback is invoked when it's not supposed to be.
  scoped_ptr<TestCompletionCallback> callback(new TestCompletionCallback);

  SpdyStreamRequest request;
  ASSERT_EQ(ERR_IO_PENDING,
            request.StartRequest(
                SPDY_BIDIRECTIONAL_STREAM, session, test_url_, MEDIUM,
                BoundNetLog(),
                callback->callback()));

  // Release the first one, this will allow the second to be created.
  spdy_stream1->Cancel();
  EXPECT_EQ(NULL, spdy_stream1.get());

  request.CancelRequest();
  callback.reset();

  // Should not crash when running the pending callback.
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_P(SpdySessionTest, SendInitialSettingsOnNewSession) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  SettingsMap settings;
  const SpdySettingsIds kSpdySettingsIds1 = SETTINGS_MAX_CONCURRENT_STREAMS;
  const SpdySettingsIds kSpdySettingsIds2 = SETTINGS_INITIAL_WINDOW_SIZE;
  const uint32 kInitialRecvWindowSize = 10 * 1024 * 1024;
  settings[kSpdySettingsIds1] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, kMaxConcurrentPushedStreams);
  if (spdy_util_.spdy_version() >= SPDY3) {
    settings[kSpdySettingsIds2] =
        SettingsFlagsAndValue(SETTINGS_FLAG_NONE, kInitialRecvWindowSize);
  }
  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  MockWrite writes[] = {
    CreateMockWrite(*settings_frame),
  };
  session_deps_.stream_initial_recv_window_size = kInitialRecvWindowSize;

  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  SpdySessionPoolPeer pool_peer(spdy_session_pool_);
  pool_peer.EnableSendingInitialSettings(true);

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdySessionTest, SendSettingsOnNewSession) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  // Create the bogus setting that we want to verify is sent out.
  // Note that it will be marked as SETTINGS_FLAG_PERSISTED when sent out. But
  // to persist it into the HttpServerProperties, we need to mark as
  // SETTINGS_FLAG_PLEASE_PERSIST.
  SettingsMap settings;
  const SpdySettingsIds kSpdySettingsIds1 = SETTINGS_UPLOAD_BANDWIDTH;
  const uint32 kBogusSettingValue = 0xCDCD;
  settings[kSpdySettingsIds1] =
      SettingsFlagsAndValue(SETTINGS_FLAG_PERSISTED, kBogusSettingValue);
  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  MockWrite writes[] = {
    CreateMockWrite(*settings_frame),
  };

  StaticSocketDataProvider data(
      reads, arraysize(reads), writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  spdy_session_pool_->http_server_properties()->SetSpdySetting(
      test_host_port_pair_,
      kSpdySettingsIds1,
      SETTINGS_FLAG_PLEASE_PERSIST,
      kBogusSettingValue);

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_TRUE(data.at_write_eof());
}

namespace {

// Specifies the style for closing the connection.
enum SpdyPoolCloseSessionsType {
  SPDY_POOL_CLOSE_SESSIONS_MANUALLY,
  SPDY_POOL_CLOSE_CURRENT_SESSIONS,
  SPDY_POOL_CLOSE_IDLE_SESSIONS,
};

// Initialize the SpdySession with socket.
void IPPoolingInitializedSession(
    const std::string& group_name,
    const scoped_refptr<TransportSocketParams>& transport_params,
    HttpNetworkSession* http_session,
    SpdySession* session) {
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(group_name,
                                 transport_params, MEDIUM, CompletionCallback(),
                                 http_session->GetTransportSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));
  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));
}

// This test has three variants, one for each style of closing the connection.
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_SESSIONS_MANUALLY,
// the sessions are closed manually, calling SpdySessionPool::Remove() directly.
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_CURRENT_SESSIONS,
// sessions are closed with SpdySessionPool::CloseCurrentSessions().
// If |clean_via_close_current_sessions| is SPDY_POOL_CLOSE_IDLE_SESSIONS,
// sessions are closed with SpdySessionPool::CloseIdleSessions().
void IPPoolingTest(NextProto next_proto,
                   SpdyPoolCloseSessionsType close_sessions_type) {
  const int kTestPort = 80;
  struct TestHosts {
    std::string url;
    std::string name;
    std::string iplist;
    SpdySessionKey key;
    AddressList addresses;
  } test_hosts[] = {
    { "http:://www.foo.com",
      "www.foo.com",
      "192.0.2.33,192.168.0.1,192.168.0.5"
    },
    { "http://js.foo.com",
      "js.foo.com",
      "192.168.0.2,192.168.0.3,192.168.0.5,192.0.2.33"
    },
    { "http://images.foo.com",
      "images.foo.com",
      "192.168.0.4,192.168.0.3"
    },
  };

  SpdySessionDependencies session_deps(next_proto);
  session_deps.host_resolver->set_synchronous_mode(true);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_hosts); i++) {
    session_deps.host_resolver->rules()->AddIPLiteralRule(
        test_hosts[i].name, test_hosts[i].iplist, std::string());

    // This test requires that the HostResolver cache be populated.  Normal
    // code would have done this already, but we do it manually.
    HostResolver::RequestInfo info(HostPortPair(test_hosts[i].name, kTestPort));
    session_deps.host_resolver->Resolve(
        info, &test_hosts[i].addresses, CompletionCallback(), NULL,
        BoundNetLog());

    // Setup a SpdySessionKey
    test_hosts[i].key = SpdySessionKey(
        HostPortPair(test_hosts[i].name, kTestPort), ProxyServer::Direct(),
        kPrivacyModeDisabled);
  }

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps.socket_factory->AddSSLSocketDataProvider(&ssl);

  scoped_refptr<HttpNetworkSession> http_session(
      SpdySessionDependencies::SpdyCreateSession(&session_deps));

  // Setup the first session to the first host.
  SpdySessionPool* spdy_session_pool(http_session->spdy_session_pool());
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[0].key));
  scoped_refptr<SpdySession> session =
      spdy_session_pool->Get(test_hosts[0].key, BoundNetLog());
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[0].key));

  HostPortPair test_host_port_pair(test_hosts[0].name, kTestPort);

  // Initialize session for the first host.
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false,
                                OnHostResolutionCallback()));
  IPPoolingInitializedSession(test_host_port_pair.ToString(),
                              transport_params,
                              http_session.get(),
                              session.get());

  // TODO(rtenneti): MockClientSocket::GetPeerAddress return's 0 as the port
  // number. Fix it to return port 80 and then use GetPeerAddress to AddAlias.
  SpdySessionPoolPeer pool_peer(spdy_session_pool);
  pool_peer.AddAlias(test_hosts[0].addresses.front(), test_hosts[0].key);

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  // The third host has no overlap with the first, so it can't pool IPs.
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[2].key));

  // The second host overlaps with the first, and should IP pool.
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[1].key));

  // Verify that the second host, through a proxy, won't share the IP.
  SpdySessionKey proxy_key(test_hosts[1].key.host_port_pair(),
      ProxyServer::FromPacString("HTTP http://proxy.foo.com/"),
      kPrivacyModeDisabled);
  EXPECT_FALSE(spdy_session_pool->HasSession(proxy_key));

  // Overlap between 2 and 3 does is not transitive to 1.
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[2].key));

  // Create a new session to host 2.
  scoped_refptr<SpdySession> session2 =
      spdy_session_pool->Get(test_hosts[2].key, BoundNetLog());

  // Verify that we have sessions for everything.
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[0].key));
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[1].key));
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[2].key));

  // Initialize session for host 2.
  session_deps.socket_factory->AddSocketDataProvider(&data);
  IPPoolingInitializedSession(test_hosts[2].key.host_port_pair().ToString(),
                              transport_params,
                              http_session.get(),
                              session2.get());

  // Grab the session to host 1 and verify that it is the same session
  // we got with host 0, and that is a different than host 2's session.
  scoped_refptr<SpdySession> session1 =
      spdy_session_pool->Get(test_hosts[1].key, BoundNetLog());
  EXPECT_EQ(session.get(), session1.get());
  EXPECT_NE(session2.get(), session1.get());

  // Initialize session for host 1.
  session_deps.socket_factory->AddSocketDataProvider(&data);
  IPPoolingInitializedSession(test_hosts[2].key.host_port_pair().ToString(),
                              transport_params,
                              http_session.get(),
                              session2.get());

  // Remove the aliases and observe that we still have a session for host1.
  pool_peer.RemoveAliases(test_hosts[0].key);
  pool_peer.RemoveAliases(test_hosts[1].key);
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[1].key));

  // Expire the host cache
  session_deps.host_resolver->GetHostCache()->clear();
  EXPECT_TRUE(spdy_session_pool->HasSession(test_hosts[1].key));

  // Cleanup the sessions.
  switch (close_sessions_type) {
    case SPDY_POOL_CLOSE_SESSIONS_MANUALLY:
      spdy_session_pool->Remove(session);
      session = NULL;
      spdy_session_pool->Remove(session2);
      session2 = NULL;
      break;
    case SPDY_POOL_CLOSE_CURRENT_SESSIONS:
      spdy_session_pool->CloseCurrentSessions(net::ERR_ABORTED);
      break;
    case SPDY_POOL_CLOSE_IDLE_SESSIONS:
      GURL url(test_hosts[0].url);
      base::WeakPtr<SpdyStream> spdy_stream =
          CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                    session, url, MEDIUM, BoundNetLog());
      GURL url1(test_hosts[1].url);
      base::WeakPtr<SpdyStream> spdy_stream1 =
          CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                    session1, url1, MEDIUM, BoundNetLog());
      GURL url2(test_hosts[2].url);
      base::WeakPtr<SpdyStream> spdy_stream2 =
          CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                    session2, url2, MEDIUM, BoundNetLog());

      // Close streams to make spdy_session and spdy_session1 inactive.
      session->CloseCreatedStream(spdy_stream, OK);
      EXPECT_EQ(NULL, spdy_stream.get());
      session1->CloseCreatedStream(spdy_stream1, OK);
      EXPECT_EQ(NULL, spdy_stream1.get());

      // Check spdy_session and spdy_session1 are not closed.
      EXPECT_FALSE(session->is_active());
      EXPECT_FALSE(session->IsClosed());
      EXPECT_FALSE(session1->is_active());
      EXPECT_FALSE(session1->IsClosed());
      EXPECT_TRUE(session2->is_active());
      EXPECT_FALSE(session2->IsClosed());

      // Test that calling CloseIdleSessions, does not cause a crash.
      // http://crbug.com/181400
      spdy_session_pool->CloseIdleSessions();

      // Verify spdy_session and spdy_session1 are closed.
      EXPECT_FALSE(session->is_active());
      EXPECT_TRUE(session->IsClosed());
      EXPECT_FALSE(session1->is_active());
      EXPECT_TRUE(session1->IsClosed());
      EXPECT_TRUE(session2->is_active());
      EXPECT_FALSE(session2->IsClosed());

      spdy_stream2->Cancel();
      EXPECT_EQ(NULL, spdy_stream.get());
      EXPECT_EQ(NULL, spdy_stream1.get());
      EXPECT_EQ(NULL, spdy_stream2.get());
      spdy_session_pool->Remove(session2);
      session2 = NULL;
      break;
  }

  // Verify that the map is all cleaned up.
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[0].key));
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[1].key));
  EXPECT_FALSE(spdy_session_pool->HasSession(test_hosts[2].key));
}

}  // namespace

TEST_P(SpdySessionTest, IPPooling) {
  IPPoolingTest(GetParam(), SPDY_POOL_CLOSE_SESSIONS_MANUALLY);
}

TEST_P(SpdySessionTest, IPPoolingCloseCurrentSessions) {
  IPPoolingTest(GetParam(), SPDY_POOL_CLOSE_CURRENT_SESSIONS);
}

TEST_P(SpdySessionTest, IPPoolingCloseIdleSessions) {
  IPPoolingTest(GetParam(), SPDY_POOL_CLOSE_IDLE_SESSIONS);
}

TEST_P(SpdySessionTest, ClearSettingsStorageOnIPAddressChanged) {
  if (GetParam() > kProtoSPDY3)
    return;

  CreateNetworkSession();

  HttpServerProperties* test_http_server_properties =
      spdy_session_pool_->http_server_properties();
  SettingsFlagsAndValue flags_and_value1(SETTINGS_FLAG_PLEASE_PERSIST, 2);
  test_http_server_properties->SetSpdySetting(
      test_host_port_pair_,
      SETTINGS_MAX_CONCURRENT_STREAMS,
      SETTINGS_FLAG_PLEASE_PERSIST,
      2);
  EXPECT_NE(0u, test_http_server_properties->GetSpdySettings(
      test_host_port_pair_).size());
  spdy_session_pool_->OnIPAddressChanged();
  EXPECT_EQ(0u, test_http_server_properties->GetSpdySettings(
      test_host_port_pair_).size());
}

TEST_P(SpdySessionTest, Initialize) {
  if (GetParam() > kProtoSPDY3)
    return;

  CapturingBoundNetLog log;
  session_deps_.net_log = log.bound().net_log();
  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  scoped_refptr<SpdySession> session =
      spdy_session_pool_->Get(key_, log.bound());
  EXPECT_TRUE(spdy_session_pool_->HasSession(key_));

  EXPECT_EQ(OK,
            InitializeSession(
                http_session_.get(), session.get(), test_host_port_pair_));

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged TYPE_SPDY_SESSION_INITIALIZED correctly.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_SPDY_SESSION_INITIALIZED,
      net::NetLog::PHASE_NONE);
  EXPECT_LT(0, pos);

  CapturingNetLog::CapturedEntry entry = entries[pos];
  NetLog::Source socket_source;
  EXPECT_TRUE(NetLog::Source::FromEventParameters(entry.params.get(),
                                                  &socket_source));
  EXPECT_TRUE(socket_source.IsValid());
  EXPECT_NE(log.bound().source().id, socket_source.id);
}

TEST_P(SpdySessionTest, CloseSessionOnError) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> goaway(spdy_util_.ConstructSpdyGoAway());
  MockRead reads[] = {
    CreateMockRead(*goaway),
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  CapturingBoundNetLog log;
  scoped_refptr<SpdySession> session =
      spdy_session_pool_->Get(key_, log.bound());
  EXPECT_TRUE(spdy_session_pool_->HasSession(key_));

  EXPECT_EQ(OK,
            InitializeSession(
                http_session_.get(), session.get(), test_host_port_pair_));

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_FALSE(spdy_session_pool_->HasSession(key_));

  // Check that the NetLog was filled reasonably.
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged SPDY_SESSION_CLOSE correctly.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_SPDY_SESSION_CLOSE,
      net::NetLog::PHASE_NONE);

  CapturingNetLog::CapturedEntry entry = entries[pos];
  int error_code = 0;
  ASSERT_TRUE(entry.GetNetErrorCode(&error_code));
  EXPECT_EQ(ERR_CONNECTION_CLOSED, error_code);
}

// Queue up a low-priority SYN_STREAM followed by a high-priority
// one. The high priority one should still send first and receive
// first.
TEST_P(SpdySessionTest, OutOfOrderSynStreams) {
  if (GetParam() > kProtoSPDY3)
    return;

  // Construct the request.
  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> req_highest(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, HIGHEST, true));
  scoped_ptr<SpdyFrame> req_lowest(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  MockWrite writes[] = {
    CreateMockWrite(*req_highest, 0),
    CreateMockWrite(*req_lowest, 1),
  };

  scoped_ptr<SpdyFrame> resp_highest(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body_highest(
      spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> resp_lowest(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body_lowest(
      spdy_util_.ConstructSpdyBodyFrame(3, true));
  MockRead reads[] = {
    CreateMockRead(*resp_highest, 2),
    CreateMockRead(*body_highest, 3),
    CreateMockRead(*resp_lowest, 4),
    CreateMockRead(*body_lowest, 5),
    MockRead(ASYNC, 0, 6)  // EOF
  };

  session_deps_.host_resolver->set_synchronous_mode(true);

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url("http://www.google.com");

  base::WeakPtr<SpdyStream> spdy_stream_lowest =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream_lowest);
  EXPECT_EQ(0u, spdy_stream_lowest->stream_id());
  test::StreamDelegateDoNothing delegate_lowest(spdy_stream_lowest);
  spdy_stream_lowest->SetDelegate(&delegate_lowest);

  base::WeakPtr<SpdyStream> spdy_stream_highest =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, HIGHEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream_highest);
  EXPECT_EQ(0u, spdy_stream_highest->stream_id());
  test::StreamDelegateDoNothing delegate_highest(spdy_stream_highest);
  spdy_stream_highest->SetDelegate(&delegate_highest);

  // Queue the lower priority one first.

  scoped_ptr<SpdyHeaderBlock> headers_lowest(
      spdy_util_.ConstructGetHeaderBlock(url.spec()));
  spdy_stream_lowest->SendRequestHeaders(
      headers_lowest.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream_lowest->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers_highest(
      spdy_util_.ConstructGetHeaderBlock(url.spec()));
  spdy_stream_highest->SendRequestHeaders(
      headers_highest.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream_highest->HasUrl());

  data.RunFor(7);

  EXPECT_FALSE(spdy_stream_lowest);
  EXPECT_FALSE(spdy_stream_highest);
  EXPECT_EQ(3u, delegate_lowest.stream_id());
  EXPECT_EQ(1u, delegate_highest.stream_id());
}

TEST_P(SpdySessionTest, CancelStream) {
  if (GetParam() > kProtoSPDY3)
    return;

  MockConnect connect_data(SYNCHRONOUS, OK);
  // Request 1, at HIGHEST priority, will be cancelled before it writes data.
  // Request 2, at LOWEST priority, will be a full request and will be id 1.
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  MockWrite writes[] = {
    CreateMockWrite(*req2, 0),
  };

  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(1, true));
  MockRead reads[] = {
    CreateMockRead(*resp2, 1),
    CreateMockRead(*body2, 2),
    MockRead(ASYNC, 0, 3)  // EOF
  };

  session_deps_.host_resolver->set_synchronous_mode(true);

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url1, HIGHEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  GURL url2("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url2, LOWEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream2.get() != NULL);
  EXPECT_EQ(0u, spdy_stream2->stream_id());
  test::StreamDelegateDoNothing delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlock(url2.spec()));
  spdy_stream2->SendRequestHeaders(headers2.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream2->HasUrl());

  EXPECT_EQ(0u, spdy_stream1->stream_id());

  spdy_stream1->Cancel();
  EXPECT_EQ(NULL, spdy_stream1.get());

  EXPECT_EQ(0u, delegate1.stream_id());

  data.RunFor(1);

  EXPECT_EQ(0u, delegate1.stream_id());
  EXPECT_EQ(1u, delegate2.stream_id());

  spdy_stream2->Cancel();
  EXPECT_EQ(NULL, spdy_stream2.get());
}

// Create two streams that are set to re-close themselves on close,
// and then close the session. Nothing should blow up. Also a
// regression test for http://crbug.com/139518 .
TEST_P(SpdySessionTest, CloseSessionWithTwoCreatedSelfClosingStreams) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);

  // No actual data will be sent.
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, 1)  // EOF
  };

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, url1, HIGHEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  GURL url2("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, url2, LOWEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream2.get() != NULL);
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  test::ClosingDelegate delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  test::ClosingDelegate delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlock(url2.spec()));
  spdy_stream2->SendRequestHeaders(headers2.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream2->HasUrl());

  // Ensure that the streams have not yet been activated and assigned an id.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Ensure we don't crash while closing the session.
  session->CloseSessionOnError(ERR_ABORTED, true, std::string());

  EXPECT_EQ(NULL, spdy_stream1.get());
  EXPECT_EQ(NULL, spdy_stream2.get());

  EXPECT_TRUE(delegate1.StreamIsClosed());
  EXPECT_TRUE(delegate2.StreamIsClosed());

  session = NULL;
}

// Create two streams that are set to close each other on close, and
// then close the session. Nothing should blow up.
TEST_P(SpdySessionTest, CloseSessionWithTwoCreatedMutuallyClosingStreams) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);

  // No actual data will be sent.
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, 1)  // EOF
  };

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, url1, HIGHEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  GURL url2("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, url2, LOWEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream2.get() != NULL);
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Make |spdy_stream1| close |spdy_stream2|.
  test::ClosingDelegate delegate1(spdy_stream2);
  spdy_stream1->SetDelegate(&delegate1);

  // Make |spdy_stream2| close |spdy_stream1|.
  test::ClosingDelegate delegate2(spdy_stream1);
  spdy_stream2->SetDelegate(&delegate2);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlock(url2.spec()));
  spdy_stream2->SendRequestHeaders(headers2.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream2->HasUrl());

  // Ensure that the streams have not yet been activated and assigned an id.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Ensure we don't crash while closing the session.
  session->CloseSessionOnError(ERR_ABORTED, true, std::string());

  EXPECT_EQ(NULL, spdy_stream1.get());
  EXPECT_EQ(NULL, spdy_stream2.get());

  EXPECT_TRUE(delegate1.StreamIsClosed());
  EXPECT_TRUE(delegate2.StreamIsClosed());

  session = NULL;
}

// Create two streams that are set to re-close themselves on close,
// activate them, and then close the session. Nothing should blow up.
TEST_P(SpdySessionTest, CloseSessionWithTwoActivatedSelfClosingStreams) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);

  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, MEDIUM, true));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, MEDIUM, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
    CreateMockWrite(*req2, 1),
  };

  MockRead reads[] = {
    MockRead(ASYNC, 0, 2)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url1, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  GURL url2("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url2, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream2.get() != NULL);
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  test::ClosingDelegate delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  test::ClosingDelegate delegate2(spdy_stream2);
  spdy_stream2->SetDelegate(&delegate2);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlock(url2.spec()));
  spdy_stream2->SendRequestHeaders(headers2.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream2->HasUrl());

  // Ensure that the streams have not yet been activated and assigned an id.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  data.RunFor(2);

  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  // Ensure we don't crash while closing the session.
  session->CloseSessionOnError(ERR_ABORTED, true, std::string());

  EXPECT_EQ(NULL, spdy_stream1.get());
  EXPECT_EQ(NULL, spdy_stream2.get());

  EXPECT_TRUE(delegate1.StreamIsClosed());
  EXPECT_TRUE(delegate2.StreamIsClosed());

  session = NULL;
}

// Create two streams that are set to close each other on close,
// activate them, and then close the session. Nothing should blow up.
TEST_P(SpdySessionTest, CloseSessionWithTwoActivatedMutuallyClosingStreams) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);

  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, MEDIUM, true));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, MEDIUM, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
    CreateMockWrite(*req2, 1),
  };

  MockRead reads[] = {
    MockRead(ASYNC, 0, 2)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url1, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  GURL url2("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url2, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream2.get() != NULL);
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  // Make |spdy_stream1| close |spdy_stream2|.
  test::ClosingDelegate delegate1(spdy_stream2);
  spdy_stream1->SetDelegate(&delegate1);

  // Make |spdy_stream2| close |spdy_stream1|.
  test::ClosingDelegate delegate2(spdy_stream1);
  spdy_stream2->SetDelegate(&delegate2);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlock(url2.spec()));
  spdy_stream2->SendRequestHeaders(headers2.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream2->HasUrl());

  // Ensure that the streams have not yet been activated and assigned an id.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  EXPECT_EQ(0u, spdy_stream2->stream_id());

  data.RunFor(2);

  EXPECT_EQ(1u, spdy_stream1->stream_id());
  EXPECT_EQ(3u, spdy_stream2->stream_id());

  // Ensure we don't crash while closing the session.
  session->CloseSessionOnError(ERR_ABORTED, true, std::string());

  EXPECT_EQ(NULL, spdy_stream1.get());
  EXPECT_EQ(NULL, spdy_stream2.get());

  EXPECT_TRUE(delegate1.StreamIsClosed());
  EXPECT_TRUE(delegate2.StreamIsClosed());

  session = NULL;
}

TEST_P(SpdySessionTest, VerifyDomainAuthentication) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);

  // No actual data will be sent.
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, 1)  // EOF
  };

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "spdy_pooling.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.cert = test_cert;
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = GetSession(key_);

  SSLConfig ssl_config;
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair_,
                                MEDIUM,
                                false,
                                false,
                                OnHostResolutionCallback()));
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SSLSocketParams> ssl_params(
      new SSLSocketParams(transport_params,
                          socks_params,
                          http_proxy_params,
                          ProxyServer::SCHEME_DIRECT,
                          test_host_port_pair_,
                          ssl_config,
                          0,
                          false,
                          false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair_.ToString(),
                                 ssl_params, MEDIUM, CompletionCallback(),
                                 http_session_->GetSSLSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));

  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));
  EXPECT_TRUE(session->VerifyDomainAuthentication("www.example.org"));
  EXPECT_TRUE(session->VerifyDomainAuthentication("mail.example.org"));
  EXPECT_TRUE(session->VerifyDomainAuthentication("mail.example.com"));
  EXPECT_FALSE(session->VerifyDomainAuthentication("mail.google.com"));
}

TEST_P(SpdySessionTest, ConnectionPooledWithTlsChannelId) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);

  // No actual data will be sent.
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, 1)  // EOF
  };

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0)  // EOF
  };
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  // Load a cert that is valid for:
  //   www.example.org
  //   mail.example.org
  //   www.example.com
  base::FilePath certs_dir = GetTestCertsDirectory();
  scoped_refptr<X509Certificate> test_cert(
      ImportCertFromFile(certs_dir, "spdy_pooling.pem"));
  ASSERT_NE(static_cast<X509Certificate*>(NULL), test_cert);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.channel_id_sent = true;
  ssl.cert = test_cert;
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = GetSession(key_);

  SSLConfig ssl_config;
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair_,
                                MEDIUM,
                                false,
                                false,
                                OnHostResolutionCallback()));
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SSLSocketParams> ssl_params(
      new SSLSocketParams(transport_params,
                          socks_params,
                          http_proxy_params,
                          ProxyServer::SCHEME_DIRECT,
                          test_host_port_pair_,
                          ssl_config,
                          0,
                          false,
                          false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair_.ToString(),
                                 ssl_params, MEDIUM, CompletionCallback(),
                                 http_session_->GetSSLSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));

  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), false, OK));
  EXPECT_TRUE(session->VerifyDomainAuthentication("www.example.org"));
  EXPECT_TRUE(session->VerifyDomainAuthentication("mail.example.org"));
  EXPECT_FALSE(session->VerifyDomainAuthentication("mail.example.com"));
  EXPECT_FALSE(session->VerifyDomainAuthentication("mail.google.com"));
}

TEST_P(SpdySessionTest, CloseTwoStalledCreateStream) {
  if (GetParam() > kProtoSPDY3)
    return;

  // TODO(rtenneti): Define a helper class/methods and move the common code in
  // this file.
  MockConnect connect_data(SYNCHRONOUS, OK);

  SettingsMap new_settings;
  const SpdySettingsIds kSpdySettingsIds1 = SETTINGS_MAX_CONCURRENT_STREAMS;
  const uint32 max_concurrent_streams = 1;
  new_settings[kSpdySettingsIds1] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, max_concurrent_streams);

  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 3, LOWEST, true));
  scoped_ptr<SpdyFrame> req3(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 5, LOWEST, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 1),
    CreateMockWrite(*req2, 4),
    CreateMockWrite(*req3, 7),
  };

  // Set up the socket so we read a SETTINGS frame that sets max concurrent
  // streams to 1.
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(new_settings));

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(spdy_util_.ConstructSpdyBodyFrame(1, true));

  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(spdy_util_.ConstructSpdyBodyFrame(3, true));

  scoped_ptr<SpdyFrame> resp3(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 5));
  scoped_ptr<SpdyFrame> body3(spdy_util_.ConstructSpdyBodyFrame(5, true));

  MockRead reads[] = {
    CreateMockRead(*settings_frame),
    CreateMockRead(*resp1, 2),
    CreateMockRead(*body1, 3),
    CreateMockRead(*resp2, 5),
    CreateMockRead(*body2, 6),
    CreateMockRead(*resp3, 8),
    CreateMockRead(*body3, 9),
    MockRead(ASYNC, 0, 10)  // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  // Read the settings frame.
  data.RunFor(1);

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url1, LOWEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  TestCompletionCallback callback2;
  GURL url2("http://www.google.com");
  SpdyStreamRequest request2;
  ASSERT_EQ(ERR_IO_PENDING,
            request2.StartRequest(
                SPDY_REQUEST_RESPONSE_STREAM,
                session, url2, LOWEST, BoundNetLog(), callback2.callback()));

  TestCompletionCallback callback3;
  GURL url3("http://www.google.com");
  SpdyStreamRequest request3;
  ASSERT_EQ(ERR_IO_PENDING,
            request3.StartRequest(
                SPDY_REQUEST_RESPONSE_STREAM,
                session, url3, LOWEST, BoundNetLog(), callback3.callback()));

  EXPECT_EQ(1u, session->num_active_streams() + session->num_created_streams());
  EXPECT_EQ(2u, session->pending_create_stream_queues(LOWEST));

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  // Run until 1st stream is closed and 2nd one is opened.
  EXPECT_EQ(0u, delegate1.stream_id());
  data.RunFor(3);
  // Pump loop for SpdySession::ProcessPendingStreamRequests().
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(NULL, spdy_stream1.get());
  EXPECT_EQ(1u, delegate1.stream_id());
  EXPECT_EQ(2u, session->num_active_streams() + session->num_created_streams());
  EXPECT_EQ(0u, session->pending_create_stream_queues(LOWEST));

  base::WeakPtr<SpdyStream> stream2 = request2.ReleaseStream();
  test::StreamDelegateDoNothing delegate2(stream2);
  stream2->SetDelegate(&delegate2);
  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructGetHeaderBlock(url2.spec()));
  stream2->SendRequestHeaders(headers2.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(stream2->HasUrl());

  // Run until 2nd stream is closed.
  EXPECT_EQ(0u, delegate2.stream_id());
  data.RunFor(3);
  EXPECT_EQ(NULL, stream2.get());
  EXPECT_EQ(3u, delegate2.stream_id());
  EXPECT_EQ(1u, session->num_active_streams() + session->num_created_streams());
  EXPECT_EQ(0u, session->pending_create_stream_queues(LOWEST));

  base::WeakPtr<SpdyStream> stream3 = request3.ReleaseStream();
  test::StreamDelegateDoNothing delegate3(stream3);
  stream3->SetDelegate(&delegate3);
  scoped_ptr<SpdyHeaderBlock> headers3(
      spdy_util_.ConstructGetHeaderBlock(url3.spec()));
  stream3->SendRequestHeaders(headers3.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(stream3->HasUrl());

  EXPECT_EQ(0u, delegate3.stream_id());
  data.RunFor(4);
  EXPECT_EQ(NULL, stream3.get());
  EXPECT_EQ(5u, delegate3.stream_id());
  EXPECT_EQ(0u, session->num_active_streams() + session->num_created_streams());
  EXPECT_EQ(0u, session->pending_create_stream_queues(LOWEST));
}

TEST_P(SpdySessionTest, CancelTwoStalledCreateStream) {
  if (GetParam() > kProtoSPDY3)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };

  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  MockConnect connect_data(SYNCHRONOUS, OK);

  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  // Initialize the SpdySetting with 1 max concurrent streams.
  spdy_session_pool_->http_server_properties()->SetSpdySetting(
      test_host_port_pair_,
      SETTINGS_MAX_CONCURRENT_STREAMS,
      SETTINGS_FLAG_PLEASE_PERSIST,
      1);

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, url1, LOWEST, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  TestCompletionCallback callback2;
  GURL url2("http://www.google.com");
  SpdyStreamRequest request2;
  ASSERT_EQ(ERR_IO_PENDING,
            request2.StartRequest(
                SPDY_BIDIRECTIONAL_STREAM, session, url2, LOWEST, BoundNetLog(),
                callback2.callback()));

  TestCompletionCallback callback3;
  GURL url3("http://www.google.com");
  SpdyStreamRequest request3;
  ASSERT_EQ(ERR_IO_PENDING,
            request3.StartRequest(
                SPDY_BIDIRECTIONAL_STREAM, session, url3, LOWEST, BoundNetLog(),
                callback3.callback()));

  EXPECT_EQ(1u, session->num_active_streams() + session->num_created_streams());
  EXPECT_EQ(2u, session->pending_create_stream_queues(LOWEST));

  // Cancel the first stream, this will allow the second stream to be created.
  EXPECT_TRUE(spdy_stream1.get() != NULL);
  spdy_stream1->Cancel();
  EXPECT_EQ(NULL, spdy_stream1.get());

  callback2.WaitForResult();
  EXPECT_EQ(2u, session->num_active_streams() + session->num_created_streams());
  EXPECT_EQ(0u, session->pending_create_stream_queues(LOWEST));

  // Cancel the second stream, this will allow the third stream to be created.
  base::WeakPtr<SpdyStream> spdy_stream2 = request2.ReleaseStream();
  spdy_stream2->Cancel();
  EXPECT_EQ(NULL, spdy_stream2.get());
  EXPECT_EQ(1u, session->num_active_streams() + session->num_created_streams());
  EXPECT_EQ(0u, session->pending_create_stream_queues(LOWEST));

  // Cancel the third stream.
  base::WeakPtr<SpdyStream> spdy_stream3 = request3.ReleaseStream();
  spdy_stream3->Cancel();
  EXPECT_EQ(NULL, spdy_stream3.get());
  EXPECT_EQ(0u, session->num_active_streams() + session->num_created_streams());
  EXPECT_EQ(0u, session->pending_create_stream_queues(LOWEST));
}

TEST_P(SpdySessionTest, NeedsCredentials) {
  if (GetParam() > kProtoSPDY3)
    return;

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.channel_id_sent = true;
  ssl.protocol_negotiated = GetParam();
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  const GURL url("https://www.foo.com");
  HostPortPair test_host_port_pair(url.host(), 443);
  SpdySessionKey key(test_host_port_pair, ProxyServer::Direct(),
                     kPrivacyModeDisabled);

  scoped_refptr<SpdySession> session = GetSession(key);

  SSLConfig ssl_config;
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false,
                                OnHostResolutionCallback()));
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SSLSocketParams> ssl_params(
      new SSLSocketParams(transport_params,
                          socks_params,
                          http_proxy_params,
                          ProxyServer::SCHEME_DIRECT,
                          test_host_port_pair,
                          ssl_config,
                          0,
                          false,
                          false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 ssl_params, MEDIUM, CompletionCallback(),
                                 http_session_->GetSSLSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));

  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), true, OK));

  EXPECT_EQ(spdy_util_.spdy_version() >= SPDY3, session->NeedsCredentials());

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  spdy_session_pool_->Remove(session);
}

TEST_P(SpdySessionTest, SendCredentials) {
  if (GetParam() > kProtoSPDY3)
    return;

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  SettingsMap settings;
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(settings));
  MockWrite writes[] = {
    CreateMockWrite(*settings_frame),
  };
  StaticSocketDataProvider data(reads, arraysize(reads),
                                writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.channel_id_sent = true;
  ssl.protocol_negotiated = kProtoSPDY3;
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateNetworkSession();

  const GURL kTestUrl("https://www.foo.com");
  HostPortPair test_host_port_pair(kTestUrl.host(), 443);
  SpdySessionKey key(test_host_port_pair, ProxyServer::Direct(),
                     kPrivacyModeDisabled);

  scoped_refptr<SpdySession> session = GetSession(key);

  SSLConfig ssl_config;
  scoped_refptr<TransportSocketParams> transport_params(
      new TransportSocketParams(test_host_port_pair,
                                MEDIUM,
                                false,
                                false,
                                OnHostResolutionCallback()));
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SSLSocketParams> ssl_params(
      new SSLSocketParams(transport_params,
                          socks_params,
                          http_proxy_params,
                          ProxyServer::SCHEME_DIRECT,
                          test_host_port_pair,
                          ssl_config,
                          0,
                          false,
                          false));
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  EXPECT_EQ(OK, connection->Init(test_host_port_pair.ToString(),
                                 ssl_params, MEDIUM, CompletionCallback(),
                                 http_session_->GetSSLSocketPool(
                                     HttpNetworkSession::NORMAL_SOCKET_POOL),
                                 BoundNetLog()));

  EXPECT_EQ(OK, session->InitializeWithSocket(connection.release(), true, OK));
  EXPECT_TRUE(session->NeedsCredentials());

  // Flush the SpdySession::OnReadComplete() task.
  base::MessageLoop::current()->RunUntilIdle();

  spdy_session_pool_->Remove(session);
  EXPECT_FALSE(spdy_session_pool_->HasSession(key));
}

TEST_P(SpdySessionTest, UpdateStreamsSendWindowSize) {
  if (GetParam() != kProtoSPDY3)
    return;

  // Set SETTINGS_INITIAL_WINDOW_SIZE to a small number so that WINDOW_UPDATE
  // gets sent.
  SettingsMap new_settings;
  int32 window_size = 1;
  new_settings[SETTINGS_INITIAL_WINDOW_SIZE] =
      SettingsFlagsAndValue(SETTINGS_FLAG_NONE, window_size);

  // Set up the socket so we read a SETTINGS frame that sets
  // INITIAL_WINDOW_SIZE.
  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> settings_frame(
      spdy_util_.ConstructSpdySettings(new_settings));
  MockRead reads[] = {
    CreateMockRead(*settings_frame, 0),
    MockRead(ASYNC, 0, 1)  // EOF
  };

  session_deps_.host_resolver->set_synchronous_mode(true);

  scoped_ptr<DeterministicSocketData> data(
      new DeterministicSocketData(reads, arraysize(reads), NULL, 0));
  data->set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(data.get());

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, test_url_, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  TestCompletionCallback callback1;
  EXPECT_NE(spdy_stream1->send_window_size(), window_size);

  data->RunFor(1);  // Process the SETTINGS frame, but not the EOF
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(session->stream_initial_send_window_size(), window_size);
  EXPECT_EQ(spdy_stream1->send_window_size(), window_size);

  // Release the first one, this will allow the second to be created.
  spdy_stream1->Cancel();
  EXPECT_EQ(NULL, spdy_stream1.get());

  base::WeakPtr<SpdyStream> spdy_stream2 =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, test_url_, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream2.get() != NULL);
  EXPECT_EQ(spdy_stream2->send_window_size(), window_size);
  spdy_stream2->Cancel();
  EXPECT_EQ(NULL, spdy_stream2.get());
}

// Test that SpdySession::DoRead reads data from the socket without yielding.
// This test makes 32k - 1 bytes of data available on the socket for reading. It
// then verifies that it has read all the available data without yielding.
TEST_P(SpdySessionTest, ReadDataWithoutYielding) {
  if (GetParam() > kProtoSPDY3)
    return;

  MockConnect connect_data(SYNCHRONOUS, OK);
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, MEDIUM, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
  };

  // Build buffer of size kMaxReadBytes / 4 (-spdy_data_frame_size).
  ASSERT_EQ(32 * 1024, kMaxReadBytes);
  const int kPayloadSize =
      kMaxReadBytes / 4 - framer.GetControlFrameHeaderSize();
  TestDataStream test_stream;
  scoped_refptr<net::IOBuffer> payload(new net::IOBuffer(kPayloadSize));
  char* payload_data = payload->data();
  test_stream.GetBytes(payload_data, kPayloadSize);

  scoped_ptr<SpdyFrame> partial_data_frame(
      framer.CreateDataFrame(1, payload_data, kPayloadSize, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> finish_data_frame(
      framer.CreateDataFrame(1, payload_data, kPayloadSize - 1, DATA_FLAG_FIN));

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));

  // Write 1 byte less than kMaxReadBytes to check that DoRead reads up to 32k
  // bytes.
  MockRead reads[] = {
    CreateMockRead(*resp1, 1),
    CreateMockRead(*partial_data_frame, 2),
    CreateMockRead(*partial_data_frame, 3, SYNCHRONOUS),
    CreateMockRead(*partial_data_frame, 4, SYNCHRONOUS),
    CreateMockRead(*finish_data_frame, 5, SYNCHRONOUS),
    MockRead(ASYNC, 0, 6)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.host_resolver->set_synchronous_mode(true);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url1, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  scoped_ptr<SpdyHeaderBlock> headers1(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers1.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  // Set up the TaskObserver to verify SpdySession::DoRead doesn't post a task.
  SpdySessionTestTaskObserver observer("spdy_session.cc", "DoRead");

  // Run until 1st read.
  EXPECT_EQ(0u, delegate1.stream_id());
  data.RunFor(2);
  EXPECT_EQ(1u, delegate1.stream_id());
  EXPECT_EQ(0u, observer.executed_count());

  // Read all the data and verify SpdySession::DoRead has not posted a task.
  data.RunFor(4);
  EXPECT_EQ(NULL, spdy_stream1.get());

  // Verify task observer's executed_count is zero, which indicates DoRead read
  // all the available data.
  EXPECT_EQ(0u, observer.executed_count());
  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data.at_read_eof());
}

// Test that SpdySession::DoRead yields while reading the data. This test makes
// 32k + 1 bytes of data available on the socket for reading. It then verifies
// that DoRead has yielded even though there is data available for it to read
// (i.e, socket()->Read didn't return ERR_IO_PENDING during socket reads).
TEST_P(SpdySessionTest, TestYieldingDuringReadData) {
  if (GetParam() > kProtoSPDY3)
    return;

  MockConnect connect_data(SYNCHRONOUS, OK);
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, MEDIUM, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
  };

  // Build buffer of size kMaxReadBytes / 4 (-spdy_data_frame_size).
  ASSERT_EQ(32 * 1024, kMaxReadBytes);
  const int kPayloadSize =
      kMaxReadBytes / 4 - framer.GetControlFrameHeaderSize();
  TestDataStream test_stream;
  scoped_refptr<net::IOBuffer> payload(new net::IOBuffer(kPayloadSize));
  char* payload_data = payload->data();
  test_stream.GetBytes(payload_data, kPayloadSize);

  scoped_ptr<SpdyFrame> partial_data_frame(
      framer.CreateDataFrame(1, payload_data, kPayloadSize, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> finish_data_frame(
      framer.CreateDataFrame(1, "h", 1, DATA_FLAG_FIN));

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));

  // Write 1 byte more than kMaxReadBytes to check that DoRead yields.
  MockRead reads[] = {
    CreateMockRead(*resp1, 1),
    CreateMockRead(*partial_data_frame, 2),
    CreateMockRead(*partial_data_frame, 3, SYNCHRONOUS),
    CreateMockRead(*partial_data_frame, 4, SYNCHRONOUS),
    CreateMockRead(*partial_data_frame, 5, SYNCHRONOUS),
    CreateMockRead(*finish_data_frame, 6, SYNCHRONOUS),
    MockRead(ASYNC, 0, 7)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.host_resolver->set_synchronous_mode(true);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url1, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  scoped_ptr<SpdyHeaderBlock> headers1(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers1.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  // Set up the TaskObserver to verify SpdySession::DoRead posts a task.
  SpdySessionTestTaskObserver observer("spdy_session.cc", "DoRead");

  // Run until 1st read.
  EXPECT_EQ(0u, delegate1.stream_id());
  data.RunFor(2);
  EXPECT_EQ(1u, delegate1.stream_id());
  EXPECT_EQ(0u, observer.executed_count());

  // Read all the data and verify SpdySession::DoRead has posted a task.
  data.RunFor(6);
  EXPECT_EQ(NULL, spdy_stream1.get());

  // Verify task observer's executed_count is 1, which indicates DoRead has
  // posted only one task and thus yielded though there is data available for it
  // to read.
  EXPECT_EQ(1u, observer.executed_count());
  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data.at_read_eof());
}

// Test that SpdySession::DoRead() tests interactions of yielding + async,
// by doing the following MockReads.
//
// MockRead of SYNCHRONOUS 8K, SYNCHRONOUS 8K, SYNCHRONOUS 8K, SYNCHRONOUS 2K
// ASYNC 8K, SYNCHRONOUS 8K, SYNCHRONOUS 8K, SYNCHRONOUS 8K, SYNCHRONOUS 2K.
//
// The above reads 26K synchronously. Since that is less that 32K, we will
// attempt to read again. However, that DoRead() will return ERR_IO_PENDING
// (because of async read), so DoRead() will yield. When we come back, DoRead()
// will read the results from the async read, and rest of the data
// synchronously.
TEST_P(SpdySessionTest, TestYieldingDuringAsyncReadData) {
  if (GetParam() > kProtoSPDY3)
    return;

  MockConnect connect_data(SYNCHRONOUS, OK);
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, MEDIUM, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
  };

  // Build buffer of size kMaxReadBytes / 4 (-spdy_data_frame_size).
  ASSERT_EQ(32 * 1024, kMaxReadBytes);
  TestDataStream test_stream;
  const int kEightKPayloadSize =
      kMaxReadBytes / 4 - framer.GetControlFrameHeaderSize();
  scoped_refptr<net::IOBuffer> eightk_payload(
      new net::IOBuffer(kEightKPayloadSize));
  char* eightk_payload_data = eightk_payload->data();
  test_stream.GetBytes(eightk_payload_data, kEightKPayloadSize);

  // Build buffer of 2k size.
  TestDataStream test_stream2;
  const int kTwoKPayloadSize = kEightKPayloadSize - 6 * 1024;
  scoped_refptr<net::IOBuffer> twok_payload(
      new net::IOBuffer(kTwoKPayloadSize));
  char* twok_payload_data = twok_payload->data();
  test_stream2.GetBytes(twok_payload_data, kTwoKPayloadSize);

  scoped_ptr<SpdyFrame> eightk_data_frame(framer.CreateDataFrame(
      1, eightk_payload_data, kEightKPayloadSize, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> twok_data_frame(framer.CreateDataFrame(
      1, twok_payload_data, kTwoKPayloadSize, DATA_FLAG_NONE));
  scoped_ptr<SpdyFrame> finish_data_frame(framer.CreateDataFrame(
      1, "h", 1, DATA_FLAG_FIN));

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));

  MockRead reads[] = {
    CreateMockRead(*resp1, 1),
    CreateMockRead(*eightk_data_frame, 2),
    CreateMockRead(*eightk_data_frame, 3, SYNCHRONOUS),
    CreateMockRead(*eightk_data_frame, 4, SYNCHRONOUS),
    CreateMockRead(*twok_data_frame, 5, SYNCHRONOUS),
    CreateMockRead(*eightk_data_frame, 6, ASYNC),
    CreateMockRead(*eightk_data_frame, 7, SYNCHRONOUS),
    CreateMockRead(*eightk_data_frame, 8, SYNCHRONOUS),
    CreateMockRead(*eightk_data_frame, 9, SYNCHRONOUS),
    CreateMockRead(*twok_data_frame, 10, SYNCHRONOUS),
    CreateMockRead(*finish_data_frame, 11, SYNCHRONOUS),
    MockRead(ASYNC, 0, 12)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.host_resolver->set_synchronous_mode(true);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url1, MEDIUM, BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  scoped_ptr<SpdyHeaderBlock> headers1(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers1.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  // Set up the TaskObserver to monitor SpdySession::DoRead posting of tasks.
  SpdySessionTestTaskObserver observer("spdy_session.cc", "DoRead");

  // Run until 1st read.
  EXPECT_EQ(0u, delegate1.stream_id());
  data.RunFor(2);
  EXPECT_EQ(1u, delegate1.stream_id());
  EXPECT_EQ(0u, observer.executed_count());

  // Read all the data and verify SpdySession::DoRead has posted a task.
  data.RunFor(12);
  EXPECT_EQ(NULL, spdy_stream1.get());

  // Verify task observer's executed_count is 1, which indicates DoRead has
  // posted only one task and thus yielded though there is data available for
  // it to read.
  EXPECT_EQ(1u, observer.executed_count());
  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data.at_read_eof());
}

// Send a GoAway frame when SpdySession is in DoLoop. If scoped_refptr to
// <SpdySession> is deleted from SpdySession::DoLoop(), we get a crash because
// GoAway could delete the SpdySession from the SpdySessionPool and the last
// reference to SpdySession.
TEST_P(SpdySessionTest, GoAwayWhileInDoLoop) {
  if (GetParam() > kProtoSPDY3)
    return;

  MockConnect connect_data(SYNCHRONOUS, OK);
  BufferedSpdyFramer framer(spdy_util_.spdy_version(), false);

  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, MEDIUM, true));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
  };

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(spdy_util_.ConstructSpdyBodyFrame(1, true));
  scoped_ptr<SpdyFrame> goaway(spdy_util_.ConstructSpdyGoAway());

  MockRead reads[] = {
    CreateMockRead(*resp1, 1),
    CreateMockRead(*body1, 2),
    CreateMockRead(*goaway, 3),
    MockRead(ASYNC, 0, 4)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.host_resolver->set_synchronous_mode(true);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url1("http://www.google.com");
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url1, MEDIUM, BoundNetLog());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);
  session = NULL;
  ASSERT_TRUE(spdy_stream1.get() != NULL);
  EXPECT_EQ(0u, spdy_stream1->stream_id());

  scoped_ptr<SpdyHeaderBlock> headers1(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  spdy_stream1->SendRequestHeaders(headers1.Pass(), NO_MORE_DATA_TO_SEND);
  EXPECT_TRUE(spdy_stream1->HasUrl());

  // Run until 1st read.
  EXPECT_EQ(0u, spdy_stream1->stream_id());
  data.RunFor(1);
  EXPECT_EQ(1u, spdy_stream1->stream_id());

  // Only references to SpdySession are held by DoLoop and
  // SpdySessionPool. If DoLoop doesn't hold the reference, we get a
  // crash if SpdySession is deleted from the SpdySessionPool.

  // Run until GoAway.
  data.RunFor(4);
  EXPECT_EQ(NULL, spdy_stream1.get());
  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data.at_read_eof());
}

// Within this framework, a SpdySession should be initialized with
// flow control disabled for protocol version 2, with flow control
// enabled only for streams for protocol version 3, and with flow
// control enabled for streams and sessions for higher versions.
TEST_P(SpdySessionTest, ProtocolNegotiation) {
  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();
  scoped_refptr<SpdySession> session = GetSession(key_);

  EXPECT_EQ(SpdySession::FLOW_CONTROL_NONE, session->flow_control_state());
  EXPECT_TRUE(session->buffered_spdy_framer_ == NULL);
  EXPECT_EQ(0, session->session_send_window_size_);
  EXPECT_EQ(0, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  InitializeSession(
      http_session_.get(), session.get(), test_host_port_pair_);

  EXPECT_EQ(spdy_util_.spdy_version(),
            session->buffered_spdy_framer_->protocol_version());
  if (GetParam() == kProtoSPDY2) {
    EXPECT_EQ(SpdySession::FLOW_CONTROL_NONE, session->flow_control_state());
    EXPECT_EQ(0, session->session_send_window_size_);
    EXPECT_EQ(0, session->session_recv_window_size_);
  } else if (GetParam() == kProtoSPDY3) {
    EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM, session->flow_control_state());
    EXPECT_EQ(0, session->session_send_window_size_);
    EXPECT_EQ(0, session->session_recv_window_size_);
  } else {
    EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM_AND_SESSION,
              session->flow_control_state());
    EXPECT_EQ(kSpdySessionInitialWindowSize,
              session->session_send_window_size_);
    EXPECT_EQ(kDefaultInitialRecvWindowSize,
              session->session_recv_window_size_);
  }
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);
}

// SpdySession::{Increase,Decrease}RecvWindowSize should properly
// adjust the session receive window size when the "enable_spdy_31"
// flag is set. In addition, SpdySession::IncreaseRecvWindowSize
// should trigger sending a WINDOW_UPDATE frame for a large enough
// delta.
TEST_P(SpdySessionTest, AdjustRecvWindowSize) {
  if (GetParam() < kProtoSPDY31)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  const int32 delta_window_size = 100;

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(ASYNC, 0, 2)  // EOF
  };
  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kSpdySessionInitialWindowSize + delta_window_size));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
    CreateMockWrite(*window_update, 1),
  };
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();
  scoped_refptr<SpdySession> session = GetSession(key_);
  InitializeSession(
      http_session_.get(), session.get(), test_host_port_pair_);
  EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM_AND_SESSION,
            session->flow_control_state());

  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  session->IncreaseRecvWindowSize(delta_window_size);
  EXPECT_EQ(kDefaultInitialRecvWindowSize + delta_window_size,
            session->session_recv_window_size_);
  EXPECT_EQ(delta_window_size, session->session_unacked_recv_window_bytes_);

  // Should trigger sending a WINDOW_UPDATE frame.
  session->IncreaseRecvWindowSize(kSpdySessionInitialWindowSize);
  EXPECT_EQ(kDefaultInitialRecvWindowSize + delta_window_size +
            kSpdySessionInitialWindowSize,
            session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  data.RunFor(2);

  session->DecreaseRecvWindowSize(
      kDefaultInitialRecvWindowSize + delta_window_size +
      kSpdySessionInitialWindowSize);
  EXPECT_EQ(0, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);
}

// SpdySession::{Increase,Decrease}SendWindowSize should properly
// adjust the session send window size when the "enable_spdy_31" flag
// is set.
TEST_P(SpdySessionTest, AdjustSendWindowSize) {
  if (GetParam() < kProtoSPDY31)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, 0, 0)  // EOF
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();
  scoped_refptr<SpdySession> session = GetSession(key_);
  InitializeSession(
      http_session_.get(), session.get(), test_host_port_pair_);
  EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM_AND_SESSION,
            session->flow_control_state());

  const int32 delta_window_size = 100;

  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);

  session->IncreaseSendWindowSize(delta_window_size);
  EXPECT_EQ(kSpdySessionInitialWindowSize + delta_window_size,
            session->session_send_window_size_);

  session->DecreaseSendWindowSize(delta_window_size);
  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);
}

// Incoming data for an inactive stream should not cause the session
// receive window size to decrease.
TEST_P(SpdySessionTest, SessionFlowControlInactiveStream) {
  if (GetParam() < kProtoSPDY31)
    return;

  session_deps_.host_resolver->set_synchronous_mode(true);

  MockConnect connect_data(SYNCHRONOUS, OK);
  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyBodyFrame(1, false));
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    MockRead(ASYNC, 0, 2)  // EOF
  };
  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
  };
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();
  scoped_refptr<SpdySession> session = GetSession(key_);
  InitializeSession(
      http_session_.get(), session.get(), test_host_port_pair_);
  EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM_AND_SESSION,
            session->flow_control_state());

  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  data.RunFor(3);

  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);
}

// A delegate that drops any received data.
class DropReceivedDataDelegate : public test::StreamDelegateSendImmediate {
 public:
  DropReceivedDataDelegate(const base::WeakPtr<SpdyStream>& stream,
                           base::StringPiece data)
      : StreamDelegateSendImmediate(stream, data) {}

  virtual ~DropReceivedDataDelegate() {}

  // Drop any received data.
  virtual void OnDataReceived(scoped_ptr<SpdyBuffer> buffer) OVERRIDE {}
};

// Send data back and forth but use a delegate that drops its received
// data. The receive window should still increase to its original
// value, i.e. we shouldn't "leak" receive window bytes.
TEST_P(SpdySessionTest, SessionFlowControlNoReceiveLeaks) {
  if (GetParam() < kProtoSPDY31)
    return;

  const char kStreamUrl[] = "http://www.google.com/";

  const int32 msg_data_size = 100;
  const std::string msg_data(msg_data_size, 'a');

  MockConnect connect_data(SYNCHRONOUS, OK);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, msg_data_size, MEDIUM, NULL, 0));
  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(
          1, msg_data.data(), msg_data_size, false));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
    CreateMockWrite(*req, 1),
    CreateMockWrite(*msg, 3),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(
          1, msg_data.data(), msg_data_size, false));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId, msg_data_size));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    CreateMockRead(*echo, 4),
    MockRead(ASYNC, 0, 5)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.host_resolver->set_synchronous_mode(true);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url(kStreamUrl);
  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, url, MEDIUM, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);
  EXPECT_EQ(0u, stream->stream_id());

  DropReceivedDataDelegate delegate(stream, msg_data);
  stream->SetDelegate(&delegate);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(url.spec(), msg_data_size));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());

  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  data.RunFor(5);

  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data.at_read_eof());

  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(msg_data_size, session->session_unacked_recv_window_bytes_);

  stream->Close();
  EXPECT_EQ(NULL, stream.get());

  EXPECT_EQ(OK, delegate.WaitForClose());

  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(msg_data_size, session->session_unacked_recv_window_bytes_);
}

// Send data back and forth but close the stream before its data frame
// can be written to the socket. The send window should then increase
// to its original value, i.e. we shouldn't "leak" send window bytes.
TEST_P(SpdySessionTest, SessionFlowControlNoSendLeaks) {
  if (GetParam() < kProtoSPDY31)
    return;

  const char kStreamUrl[] = "http://www.google.com/";

  const int32 msg_data_size = 100;
  const std::string msg_data(msg_data_size, 'a');

  MockConnect connect_data(SYNCHRONOUS, OK);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, msg_data_size, MEDIUM, NULL, 0));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
    CreateMockWrite(*req, 1),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(ASYNC, 0, 3)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.host_resolver->set_synchronous_mode(true);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url(kStreamUrl);
  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, url, MEDIUM, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);
  EXPECT_EQ(0u, stream->stream_id());

  test::StreamDelegateSendImmediate delegate(stream, msg_data);
  stream->SetDelegate(&delegate);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(url.spec(), msg_data_size));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());

  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);

  data.RunFor(2);

  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);

  data.RunFor(1);

  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data.at_read_eof());

  EXPECT_EQ(kSpdySessionInitialWindowSize - msg_data_size,
            session->session_send_window_size_);

  // Closing the stream should increase the session's send window.
  stream->Close();
  EXPECT_EQ(NULL, stream.get());

  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);

  EXPECT_EQ(OK, delegate.WaitForClose());
}

// Send data back and forth; the send and receive windows should
// change appropriately.
TEST_P(SpdySessionTest, SessionFlowControlEndToEnd) {
  if (GetParam() < kProtoSPDY31)
    return;

  const char kStreamUrl[] = "http://www.google.com/";

  const int32 msg_data_size = 100;
  const std::string msg_data(msg_data_size, 'a');

  MockConnect connect_data(SYNCHRONOUS, OK);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, msg_data_size, MEDIUM, NULL, 0));
  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(
          1, msg_data.data(), msg_data_size, false));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
    CreateMockWrite(*req, 1),
    CreateMockWrite(*msg, 3),
  };

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(
          1, msg_data.data(), msg_data_size, false));
  scoped_ptr<SpdyFrame> window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId, msg_data_size));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    CreateMockRead(*echo, 4),
    CreateMockRead(*window_update, 5),
    MockRead(ASYNC, 0, 6)  // EOF
  };

  // Create SpdySession and SpdyStream and send the request.
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.host_resolver->set_synchronous_mode(true);
  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  session_deps_.deterministic_socket_factory->AddSSLSocketDataProvider(&ssl);

  CreateDeterministicNetworkSession();

  scoped_refptr<SpdySession> session = CreateInitializedSession();

  GURL url(kStreamUrl);
  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(SPDY_BIDIRECTIONAL_STREAM,
                                session, url, MEDIUM, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);
  EXPECT_EQ(0u, stream->stream_id());

  test::StreamDelegateSendImmediate delegate(stream, msg_data);
  stream->SetDelegate(&delegate);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(url.spec(), msg_data_size));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());

  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);
  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  data.RunFor(2);

  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);
  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  data.RunFor(1);

  EXPECT_EQ(kSpdySessionInitialWindowSize - msg_data_size,
            session->session_send_window_size_);
  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  data.RunFor(1);

  EXPECT_EQ(kSpdySessionInitialWindowSize - msg_data_size,
            session->session_send_window_size_);
  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  data.RunFor(1);

  EXPECT_EQ(kSpdySessionInitialWindowSize - msg_data_size,
            session->session_send_window_size_);
  EXPECT_EQ(kDefaultInitialRecvWindowSize - msg_data_size,
            session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  data.RunFor(1);

  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);
  EXPECT_EQ(kDefaultInitialRecvWindowSize - msg_data_size,
            session->session_recv_window_size_);
  EXPECT_EQ(0, session->session_unacked_recv_window_bytes_);

  EXPECT_TRUE(data.at_write_eof());
  EXPECT_TRUE(data.at_read_eof());

  EXPECT_EQ(msg_data, delegate.TakeReceivedData());

  // Draining the delegate's read queue should increase the session's
  // receive window.
  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);
  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(msg_data_size, session->session_unacked_recv_window_bytes_);

  stream->Close();
  EXPECT_EQ(NULL, stream.get());

  EXPECT_EQ(OK, delegate.WaitForClose());

  EXPECT_EQ(kSpdySessionInitialWindowSize, session->session_send_window_size_);
  EXPECT_EQ(kDefaultInitialRecvWindowSize, session->session_recv_window_size_);
  EXPECT_EQ(msg_data_size, session->session_unacked_recv_window_bytes_);
}

// Given a stall function and an unstall function, runs a test to make
// sure that a stream resumes after unstall.
void SpdySessionTest::RunResumeAfterUnstallTest(
    const base::Callback<void(SpdySession*, SpdyStream*)>& stall_fn,
    const base::Callback<void(SpdySession*, SpdyStream*, int32)>&
        unstall_function) {
  const char kStreamUrl[] = "http://www.google.com/";
  GURL url(kStreamUrl);

  session_deps_.host_resolver->set_synchronous_mode(true);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kBodyDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> body(
      spdy_util_.ConstructSpdyBodyFrame(1, kBodyData, kBodyDataSize, true));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
    CreateMockWrite(*req, 1),
    CreateMockWrite(*body, 2),
  };

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kBodyData, kBodyDataSize, false));
  MockRead reads[] = {
    CreateMockRead(*resp, 3),
    MockRead(ASYNC, 0, 0, 4), // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  CreateDeterministicNetworkSession();
  scoped_refptr<SpdySession> session = GetSession(key_);
  InitializeSession(
      http_session_.get(), session.get(), test_host_port_pair_);
  EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM_AND_SESSION,
            session->flow_control_state());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  test::StreamDelegateWithBody delegate(stream, kBodyDataStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());
  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  stall_fn.Run(session.get(), stream.get());

  data.RunFor(2);

  EXPECT_TRUE(stream->send_stalled_by_flow_control());

  unstall_function.Run(session.get(), stream.get(), kBodyDataSize);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  data.RunFor(3);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(":status"));
  EXPECT_EQ("HTTP/1.1", delegate.GetResponseHeaderValue(":version"));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

// Run the resume-after-unstall test with all possible stall and
// unstall sequences.

TEST_P(SpdySessionTest, ResumeAfterUnstallSession) {
  if (GetParam() < kProtoSPDY31)
    return;

  RunResumeAfterUnstallTest(
      base::Bind(&SpdySessionTest::StallSessionOnly,
                 base::Unretained(this)),
      base::Bind(&SpdySessionTest::UnstallSessionOnly,
                 base::Unretained(this)));
}

// Equivalent to
// SpdyStreamTest.ResumeAfterSendWindowSizeIncrease.
TEST_P(SpdySessionTest, ResumeAfterUnstallStream) {
  if (GetParam() < kProtoSPDY31)
    return;

  RunResumeAfterUnstallTest(
      base::Bind(&SpdySessionTest::StallStreamOnly,
                 base::Unretained(this)),
      base::Bind(&SpdySessionTest::UnstallStreamOnly,
                 base::Unretained(this)));
}

TEST_P(SpdySessionTest, StallSessionStreamResumeAfterUnstallSessionStream) {
  if (GetParam() < kProtoSPDY31)
    return;

  RunResumeAfterUnstallTest(
      base::Bind(&SpdySessionTest::StallSessionStream,
                 base::Unretained(this)),
      base::Bind(&SpdySessionTest::UnstallSessionStream,
                 base::Unretained(this)));
}

TEST_P(SpdySessionTest, StallStreamSessionResumeAfterUnstallSessionStream) {
  if (GetParam() < kProtoSPDY31)
    return;

  RunResumeAfterUnstallTest(
      base::Bind(&SpdySessionTest::StallStreamSession,
                 base::Unretained(this)),
      base::Bind(&SpdySessionTest::UnstallSessionStream,
                 base::Unretained(this)));
}

TEST_P(SpdySessionTest, StallStreamSessionResumeAfterUnstallStreamSession) {
  if (GetParam() < kProtoSPDY31)
    return;

  RunResumeAfterUnstallTest(
      base::Bind(&SpdySessionTest::StallStreamSession,
                 base::Unretained(this)),
      base::Bind(&SpdySessionTest::UnstallStreamSession,
                 base::Unretained(this)));
}

TEST_P(SpdySessionTest, StallSessionStreamResumeAfterUnstallStreamSession) {
  if (GetParam() < kProtoSPDY31)
    return;

  RunResumeAfterUnstallTest(
      base::Bind(&SpdySessionTest::StallSessionStream,
                 base::Unretained(this)),
      base::Bind(&SpdySessionTest::UnstallStreamSession,
                 base::Unretained(this)));
}

// Cause a stall by reducing the flow control send window to 0. The
// streams should resume in priority order when that window is then
// increased.
TEST_P(SpdySessionTest, ResumeByPriorityAfterSendWindowSizeIncrease) {
  if (GetParam() < kProtoSPDY31)
    return;

  const char kStreamUrl[] = "http://www.google.com/";
  GURL url(kStreamUrl);

  session_deps_.host_resolver->set_synchronous_mode(true);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kBodyDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 3, kBodyDataSize, MEDIUM, NULL, 0));
  scoped_ptr<SpdyFrame> body1(
      spdy_util_.ConstructSpdyBodyFrame(1, kBodyData, kBodyDataSize, true));
  scoped_ptr<SpdyFrame> body2(
      spdy_util_.ConstructSpdyBodyFrame(3, kBodyData, kBodyDataSize, true));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
    CreateMockWrite(*req1, 1),
    CreateMockWrite(*req2, 2),
    CreateMockWrite(*body2, 3),
    CreateMockWrite(*body1, 4),
  };

  scoped_ptr<SpdyFrame> resp1(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  MockRead reads[] = {
    CreateMockRead(*resp1, 5),
    CreateMockRead(*resp2, 6),
    MockRead(ASYNC, 0, 0, 7), // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  CreateDeterministicNetworkSession();
  scoped_refptr<SpdySession> session = GetSession(key_);
  InitializeSession(
      http_session_.get(), session.get(), test_host_port_pair_);
  EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM_AND_SESSION,
            session->flow_control_state());

  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream1.get() != NULL);

  test::StreamDelegateWithBody delegate1(stream1, kBodyDataStringPiece);
  stream1->SetDelegate(&delegate1);

  EXPECT_FALSE(stream1->HasUrl());

  base::WeakPtr<SpdyStream> stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, MEDIUM, BoundNetLog());
  ASSERT_TRUE(stream2.get() != NULL);

  test::StreamDelegateWithBody delegate2(stream2, kBodyDataStringPiece);
  stream2->SetDelegate(&delegate2);

  EXPECT_FALSE(stream2->HasUrl());

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  StallSessionSend(session.get());

  scoped_ptr<SpdyHeaderBlock> headers1(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream1->SendRequestHeaders(headers1.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream1->HasUrl());
  EXPECT_EQ(kStreamUrl, stream1->GetUrl().spec());

  data.RunFor(2);
  EXPECT_EQ(1u, stream1->stream_id());
  EXPECT_TRUE(stream1->send_stalled_by_flow_control());

  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream2->SendRequestHeaders(headers2.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream2->HasUrl());
  EXPECT_EQ(kStreamUrl, stream2->GetUrl().spec());

  data.RunFor(1);
  EXPECT_EQ(3u, stream2->stream_id());
  EXPECT_TRUE(stream2->send_stalled_by_flow_control());

  // This should unstall only stream2.
  UnstallSessionSend(session.get(), kBodyDataSize);

  EXPECT_TRUE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  data.RunFor(1);

  EXPECT_TRUE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  // This should then unstall stream1.
  UnstallSessionSend(session.get(), kBodyDataSize);

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  data.RunFor(4);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate1.WaitForClose());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate2.WaitForClose());

  EXPECT_TRUE(delegate1.send_headers_completed());
  EXPECT_EQ("200", delegate1.GetResponseHeaderValue(":status"));
  EXPECT_EQ("HTTP/1.1", delegate1.GetResponseHeaderValue(":version"));
  EXPECT_EQ(std::string(), delegate1.TakeReceivedData());

  EXPECT_TRUE(delegate2.send_headers_completed());
  EXPECT_EQ("200", delegate2.GetResponseHeaderValue(":status"));
  EXPECT_EQ("HTTP/1.1", delegate2.GetResponseHeaderValue(":version"));
  EXPECT_EQ(std::string(), delegate2.TakeReceivedData());

  EXPECT_TRUE(data.at_write_eof());
}

// Delegate that closes a given stream after sending its body.
class StreamClosingDelegate : public test::StreamDelegateWithBody {
 public:
  StreamClosingDelegate(const base::WeakPtr<SpdyStream>& stream,
                        base::StringPiece data)
      : StreamDelegateWithBody(stream, data) {}

  virtual ~StreamClosingDelegate() {}

  void set_stream_to_close(const base::WeakPtr<SpdyStream>& stream_to_close) {
    stream_to_close_ = stream_to_close;
  }

  virtual void OnDataSent() OVERRIDE {
    test::StreamDelegateWithBody::OnDataSent();
    if (stream_to_close_.get()) {
      stream_to_close_->Close();
      EXPECT_EQ(NULL, stream_to_close_.get());
    }
  }

 private:
  base::WeakPtr<SpdyStream> stream_to_close_;
};

// Cause a stall by reducing the flow control send window to
// 0. Unstalling the session should properly handle deleted streams.
TEST_P(SpdySessionTest, SendWindowSizeIncreaseWithDeletedStreams) {
  if (GetParam() < kProtoSPDY31)
    return;

  const char kStreamUrl[] = "http://www.google.com/";
  GURL url(kStreamUrl);

  session_deps_.host_resolver->set_synchronous_mode(true);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kBodyDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 3, kBodyDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> req3(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 5, kBodyDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> body2(
      spdy_util_.ConstructSpdyBodyFrame(3, kBodyData, kBodyDataSize, true));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
    CreateMockWrite(*req1, 1),
    CreateMockWrite(*req2, 2),
    CreateMockWrite(*req3, 3),
    CreateMockWrite(*body2, 4),
  };

  scoped_ptr<SpdyFrame> resp2(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 3));
  MockRead reads[] = {
    CreateMockRead(*resp2, 5),
    MockRead(ASYNC, 0, 0, 6), // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  CreateDeterministicNetworkSession();
  scoped_refptr<SpdySession> session = GetSession(key_);
  InitializeSession(
      http_session_.get(), session.get(), test_host_port_pair_);
  EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM_AND_SESSION,
            session->flow_control_state());

  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream1.get() != NULL);

  test::StreamDelegateWithBody delegate1(stream1, kBodyDataStringPiece);
  stream1->SetDelegate(&delegate1);

  EXPECT_FALSE(stream1->HasUrl());

  base::WeakPtr<SpdyStream> stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream2.get() != NULL);

  StreamClosingDelegate delegate2(stream2, kBodyDataStringPiece);
  stream2->SetDelegate(&delegate2);

  EXPECT_FALSE(stream2->HasUrl());

  base::WeakPtr<SpdyStream> stream3 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream3.get() != NULL);

  test::StreamDelegateWithBody delegate3(stream3, kBodyDataStringPiece);
  stream3->SetDelegate(&delegate3);

  EXPECT_FALSE(stream3->HasUrl());

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());
  EXPECT_FALSE(stream3->send_stalled_by_flow_control());

  StallSessionSend(session.get());

  scoped_ptr<SpdyHeaderBlock> headers1(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream1->SendRequestHeaders(headers1.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream1->HasUrl());
  EXPECT_EQ(kStreamUrl, stream1->GetUrl().spec());

  data.RunFor(2);
  EXPECT_EQ(1u, stream1->stream_id());
  EXPECT_TRUE(stream1->send_stalled_by_flow_control());

  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream2->SendRequestHeaders(headers2.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream2->HasUrl());
  EXPECT_EQ(kStreamUrl, stream2->GetUrl().spec());

  data.RunFor(1);
  EXPECT_EQ(3u, stream2->stream_id());
  EXPECT_TRUE(stream2->send_stalled_by_flow_control());

  scoped_ptr<SpdyHeaderBlock> headers3(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream3->SendRequestHeaders(headers3.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream3->HasUrl());
  EXPECT_EQ(kStreamUrl, stream3->GetUrl().spec());

  data.RunFor(1);
  EXPECT_EQ(5u, stream3->stream_id());
  EXPECT_TRUE(stream3->send_stalled_by_flow_control());

  SpdyStreamId stream_id1 = stream1->stream_id();
  SpdyStreamId stream_id2 = stream2->stream_id();
  SpdyStreamId stream_id3 = stream3->stream_id();

  // Close stream1 preemptively.
  session->CloseActiveStream(stream_id1, ERR_CONNECTION_CLOSED);
  EXPECT_EQ(NULL, stream1.get());

  EXPECT_FALSE(session->IsStreamActive(stream_id1));
  EXPECT_TRUE(session->IsStreamActive(stream_id2));
  EXPECT_TRUE(session->IsStreamActive(stream_id3));

  // Unstall stream2, which should then close stream3.
  delegate2.set_stream_to_close(stream3);
  UnstallSessionSend(session.get(), kBodyDataSize);

  data.RunFor(1);
  EXPECT_EQ(NULL, stream3.get());

  EXPECT_FALSE(stream2->send_stalled_by_flow_control());
  EXPECT_FALSE(session->IsStreamActive(stream_id1));
  EXPECT_TRUE(session->IsStreamActive(stream_id2));
  EXPECT_FALSE(session->IsStreamActive(stream_id3));

  data.RunFor(2);
  EXPECT_EQ(NULL, stream2.get());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate1.WaitForClose());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate2.WaitForClose());
  EXPECT_EQ(OK, delegate3.WaitForClose());

  EXPECT_TRUE(delegate1.send_headers_completed());
  EXPECT_EQ(std::string(), delegate1.TakeReceivedData());

  EXPECT_TRUE(delegate2.send_headers_completed());
  EXPECT_EQ("200", delegate2.GetResponseHeaderValue(":status"));
  EXPECT_EQ("HTTP/1.1", delegate2.GetResponseHeaderValue(":version"));
  EXPECT_EQ(std::string(), delegate2.TakeReceivedData());

  EXPECT_TRUE(delegate3.send_headers_completed());
  EXPECT_EQ(std::string(), delegate3.TakeReceivedData());

  EXPECT_TRUE(data.at_write_eof());
}

// Cause a stall by reducing the flow control send window to
// 0. Unstalling the session should properly handle the session itself
// being closed.
TEST_P(SpdySessionTest, SendWindowSizeIncreaseWithDeletedSession) {
  if (GetParam() < kProtoSPDY31)
    return;

  const char kStreamUrl[] = "http://www.google.com/";
  GURL url(kStreamUrl);

  session_deps_.host_resolver->set_synchronous_mode(true);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kBodyDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> req2(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 3, kBodyDataSize, LOWEST, NULL, 0));
  scoped_ptr<SpdyFrame> body1(
      spdy_util_.ConstructSpdyBodyFrame(1, kBodyData, kBodyDataSize, false));
  MockWrite writes[] = {
    CreateMockWrite(*initial_window_update, 0),
    CreateMockWrite(*req1, 1),
    CreateMockWrite(*req2, 2),
  };

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0, 3), // EOF
  };

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  CreateDeterministicNetworkSession();
  scoped_refptr<SpdySession> session = GetSession(key_);
  InitializeSession(
      http_session_.get(), session.get(), test_host_port_pair_);
  EXPECT_EQ(SpdySession::FLOW_CONTROL_STREAM_AND_SESSION,
            session->flow_control_state());

  base::WeakPtr<SpdyStream> stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream1.get() != NULL);

  test::StreamDelegateWithBody delegate1(stream1, kBodyDataStringPiece);
  stream1->SetDelegate(&delegate1);

  EXPECT_FALSE(stream1->HasUrl());

  base::WeakPtr<SpdyStream> stream2 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream2.get() != NULL);

  test::StreamDelegateWithBody delegate2(stream2, kBodyDataStringPiece);
  stream2->SetDelegate(&delegate2);

  EXPECT_FALSE(stream2->HasUrl());

  EXPECT_FALSE(stream1->send_stalled_by_flow_control());
  EXPECT_FALSE(stream2->send_stalled_by_flow_control());

  StallSessionSend(session.get());

  scoped_ptr<SpdyHeaderBlock> headers1(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream1->SendRequestHeaders(headers1.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream1->HasUrl());
  EXPECT_EQ(kStreamUrl, stream1->GetUrl().spec());

  data.RunFor(2);
  EXPECT_EQ(1u, stream1->stream_id());
  EXPECT_TRUE(stream1->send_stalled_by_flow_control());

  scoped_ptr<SpdyHeaderBlock> headers2(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kBodyDataSize));
  EXPECT_EQ(ERR_IO_PENDING,
            stream2->SendRequestHeaders(headers2.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream2->HasUrl());
  EXPECT_EQ(kStreamUrl, stream2->GetUrl().spec());

  data.RunFor(1);
  EXPECT_EQ(3u, stream2->stream_id());
  EXPECT_TRUE(stream2->send_stalled_by_flow_control());

  EXPECT_TRUE(spdy_session_pool_->HasSession(key_));

  // Unstall stream1.
  UnstallSessionSend(session.get(), kBodyDataSize);

  // Close the session (since we can't do it from within the delegate
  // method, since it's in the stream's loop).
  session->CloseSessionOnError(ERR_CONNECTION_CLOSED, true, "Closing session");
  session = NULL;

  EXPECT_FALSE(spdy_session_pool_->HasSession(key_));

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate1.WaitForClose());
  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate2.WaitForClose());

  EXPECT_TRUE(delegate1.send_headers_completed());
  EXPECT_EQ(std::string(), delegate1.TakeReceivedData());

  EXPECT_TRUE(delegate2.send_headers_completed());
  EXPECT_EQ(std::string(), delegate2.TakeReceivedData());

  EXPECT_TRUE(data.at_write_eof());
}

// Tests the case of a non-SPDY request closing an idle SPDY session when no
// pointers to the idle session are currently held.
TEST_P(SpdySessionTest, CloseOneIdleConnection) {
  if (GetParam() > kProtoSPDY3)
    return;

  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();

  TransportClientSocketPool* pool =
      http_session_->GetTransportSocketPool(
          HttpNetworkSession::NORMAL_SOCKET_POOL);

  // Create an idle SPDY session.
  SpdySessionKey key1(HostPortPair("1.com", 80), ProxyServer::Direct(),
                      kPrivacyModeDisabled);
  scoped_refptr<SpdySession> session1 = GetSession(key1);
  EXPECT_EQ(
      OK,
      InitializeSession(http_session_.get(), session1.get(),
                        key1.host_port_pair()));
  EXPECT_FALSE(pool->IsStalled());
  // Release the pointer to the session so it can be closed.
  session1 = NULL;

  // Trying to create a new connection should cause the pool to be stalled, and
  // post a task asynchronously to try and close the session.
  TestCompletionCallback callback2;
  HostPortPair host_port2("2.com", 80);
  scoped_refptr<TransportSocketParams> params2(
      new TransportSocketParams(host_port2, DEFAULT_PRIORITY, false, false,
                                OnHostResolutionCallback()));
  scoped_ptr<ClientSocketHandle> connection2(new ClientSocketHandle);
  EXPECT_EQ(ERR_IO_PENDING,
            connection2->Init(host_port2.ToString(), params2, DEFAULT_PRIORITY,
                              callback2.callback(), pool, BoundNetLog()));
  EXPECT_TRUE(pool->IsStalled());

  // The socket pool should close the connection asynchronously and establish a
  // new connection.
  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(pool->IsStalled());
}

// Tests the case of a non-SPDY request closing an idle SPDY session when no
// pointers to the idle session are currently held, in the case the SPDY session
// has an alias.
TEST_P(SpdySessionTest, CloseOneIdleConnectionWithAlias) {
  if (GetParam() > kProtoSPDY3)
    return;

  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  session_deps_.host_resolver->set_synchronous_mode(true);
  session_deps_.host_resolver->rules()->AddIPLiteralRule(
      "1.com", "192.168.0.2", std::string());
  session_deps_.host_resolver->rules()->AddIPLiteralRule(
      "2.com", "192.168.0.2", std::string());
  // Not strictly needed.
  session_deps_.host_resolver->rules()->AddIPLiteralRule(
      "3.com", "192.168.0.3", std::string());

  CreateNetworkSession();

  TransportClientSocketPool* pool =
      http_session_->GetTransportSocketPool(
          HttpNetworkSession::NORMAL_SOCKET_POOL);

  // Create an idle SPDY session.
  SpdySessionKey key1(HostPortPair("1.com", 80), ProxyServer::Direct(),
                      kPrivacyModeDisabled);
  scoped_refptr<SpdySession> session1 = GetSession(key1);
  EXPECT_EQ(
      OK,
      InitializeSession(http_session_.get(), session1.get(),
                        key1.host_port_pair()));
  EXPECT_FALSE(pool->IsStalled());

  // Set up an alias for the idle SPDY session, increasing its ref count to 2.
  SpdySessionKey key2(HostPortPair("2.com", 80), ProxyServer::Direct(),
                      kPrivacyModeDisabled);
  SpdySessionPoolPeer pool_peer(spdy_session_pool_);
  HostResolver::RequestInfo info(key2.host_port_pair());
  AddressList addresses;
  // Pre-populate the DNS cache, since a synchronous resolution is required in
  // order to create the alias.
  session_deps_.host_resolver->Resolve(
      info, &addresses, CompletionCallback(), NULL, BoundNetLog());
  // Add the alias for the first session's key.  Has to be done manually since
  // the usual process is bypassed.
  pool_peer.AddAlias(addresses.front(), key1);
  // Get a session for |key2|, which should return the session created earlier.
  scoped_refptr<SpdySession> session2 =
      spdy_session_pool_->Get(key2, BoundNetLog());
  ASSERT_EQ(session1.get(), session2.get());
  EXPECT_FALSE(pool->IsStalled());

  // Release both the pointers to the session so it can be closed.
  session1 = NULL;
  session2 = NULL;

  // Trying to create a new connection should cause the pool to be stalled, and
  // post a task asynchronously to try and close the session.
  TestCompletionCallback callback3;
  HostPortPair host_port3("3.com", 80);
  scoped_refptr<TransportSocketParams> params3(
      new TransportSocketParams(host_port3, DEFAULT_PRIORITY, false, false,
                                OnHostResolutionCallback()));
  scoped_ptr<ClientSocketHandle> connection3(new ClientSocketHandle);
  EXPECT_EQ(ERR_IO_PENDING,
            connection3->Init(host_port3.ToString(), params3, DEFAULT_PRIORITY,
                              callback3.callback(), pool, BoundNetLog()));
  EXPECT_TRUE(pool->IsStalled());

  // The socket pool should close the connection asynchronously and establish a
  // new connection.
  EXPECT_EQ(OK, callback3.WaitForResult());
  EXPECT_FALSE(pool->IsStalled());
}

// Tests the case of a non-SPDY request closing an idle SPDY session when a
// pointer to the idle session is still held.
TEST_P(SpdySessionTest, CloseOneIdleConnectionSessionStillHeld) {
  if (GetParam() > kProtoSPDY3)
    return;

  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  StaticSocketDataProvider data(reads, arraysize(reads), NULL, 0);
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();

  TransportClientSocketPool* pool =
      http_session_->GetTransportSocketPool(
          HttpNetworkSession::NORMAL_SOCKET_POOL);

  // Create an idle SPDY session.
  SpdySessionKey key1(HostPortPair("1.com", 80), ProxyServer::Direct(),
                      kPrivacyModeDisabled);
  scoped_refptr<SpdySession> session1 = GetSession(key1);
  EXPECT_EQ(
      OK,
      InitializeSession(http_session_.get(), session1.get(),
                        key1.host_port_pair()));
  EXPECT_FALSE(pool->IsStalled());

  // Trying to create a new connection should cause the pool to be stalled, and
  // post a task asynchronously to try and close the session.
  TestCompletionCallback callback2;
  HostPortPair host_port2("2.com", 80);
  scoped_refptr<TransportSocketParams> params2(
      new TransportSocketParams(host_port2, DEFAULT_PRIORITY, false, false,
                                OnHostResolutionCallback()));
  scoped_ptr<ClientSocketHandle> connection2(new ClientSocketHandle);
  EXPECT_EQ(ERR_IO_PENDING,
            connection2->Init(host_port2.ToString(), params2, DEFAULT_PRIORITY,
                              callback2.callback(), pool, BoundNetLog()));
  EXPECT_TRUE(pool->IsStalled());

  // Running the message loop should cause the session to prepare to be closed,
  // but since there's still an outstanding reference, it should not be closed
  // yet.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pool->IsStalled());
  EXPECT_FALSE(callback2.have_result());

  // Release the pointer to the session so it can be closed.
  session1 = NULL;
  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(pool->IsStalled());
}

// Tests that a non-SPDY request can't close a SPDY session that's currently in
// use.
TEST_P(SpdySessionTest, CloseOneIdleConnectionFailsWhenSessionInUse) {
  if (GetParam() > kProtoSPDY3)
    return;

  ClientSocketPoolManager::set_max_sockets_per_group(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);
  ClientSocketPoolManager::set_max_sockets_per_pool(
      HttpNetworkSession::NORMAL_SOCKET_POOL, 1);

  MockConnect connect_data(SYNCHRONOUS, OK);
  MockRead reads[] = {
    MockRead(SYNCHRONOUS, ERR_IO_PENDING)  // Stall forever.
  };
  scoped_ptr<SpdyFrame> req1(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  scoped_ptr<SpdyFrame> cancel1(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_CANCEL));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 1),
    CreateMockWrite(*cancel1, 1),
  };
  StaticSocketDataProvider data(reads, arraysize(reads),
                                     writes, arraysize(writes));
  data.set_connect_data(connect_data);
  session_deps_.socket_factory->AddSocketDataProvider(&data);

  CreateNetworkSession();

  TransportClientSocketPool* pool =
      http_session_->GetTransportSocketPool(
          HttpNetworkSession::NORMAL_SOCKET_POOL);

  // Create a SPDY session.
  GURL url1("http://www.google.com");
  SpdySessionKey key1(HostPortPair(url1.host(), 80),
                      ProxyServer::Direct(), kPrivacyModeDisabled);
  scoped_refptr<SpdySession> session1 = GetSession(key1);
  EXPECT_EQ(
      OK,
      InitializeSession(http_session_.get(), session1.get(),
                        key1.host_port_pair()));
  EXPECT_FALSE(pool->IsStalled());

  // Create a stream using the session, and send a request.

  TestCompletionCallback callback1;
  base::WeakPtr<SpdyStream> spdy_stream1 =
      CreateStreamSynchronously(SPDY_REQUEST_RESPONSE_STREAM,
                                session1, url1, DEFAULT_PRIORITY,
                                BoundNetLog());
  ASSERT_TRUE(spdy_stream1.get());
  test::StreamDelegateDoNothing delegate1(spdy_stream1);
  spdy_stream1->SetDelegate(&delegate1);

  scoped_ptr<SpdyHeaderBlock> headers1(
      spdy_util_.ConstructGetHeaderBlock(url1.spec()));
  EXPECT_EQ(ERR_IO_PENDING,
            spdy_stream1->SendRequestHeaders(
                headers1.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(spdy_stream1->HasUrl());

  base::MessageLoop::current()->RunUntilIdle();

  // Release the session, so holding onto a pointer here does not affect
  // anything.
  session1 = NULL;

  // Trying to create a new connection should cause the pool to be stalled, and
  // post a task asynchronously to try and close the session.
  TestCompletionCallback callback2;
  HostPortPair host_port2("2.com", 80);
  scoped_refptr<TransportSocketParams> params2(
      new TransportSocketParams(host_port2, DEFAULT_PRIORITY, false, false,
                                OnHostResolutionCallback()));
  scoped_ptr<ClientSocketHandle> connection2(new ClientSocketHandle);
  EXPECT_EQ(ERR_IO_PENDING,
            connection2->Init(host_port2.ToString(), params2, DEFAULT_PRIORITY,
                              callback2.callback(), pool, BoundNetLog()));
  EXPECT_TRUE(pool->IsStalled());

  // Running the message loop should cause the socket pool to ask the SPDY
  // session to close an idle socket, but since the socket is in use, nothing
  // happens.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pool->IsStalled());
  EXPECT_FALSE(callback2.have_result());

  // Cancelling the request should still not release the session's socket,
  // since the session is still kept alive by the SpdySessionPool.
  ASSERT_TRUE(spdy_stream1.get());
  spdy_stream1->Cancel();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(pool->IsStalled());
  EXPECT_FALSE(callback2.have_result());
}

// Verify that SpdySessionKey and therefore SpdySession is different when
// privacy mode is enabled or disabled.
TEST_P(SpdySessionTest, SpdySessionKeyPrivacyMode) {
  CreateDeterministicNetworkSession();

  HostPortPair host_port_pair("www.google.com", 443);
  SpdySessionKey key_privacy_enabled(host_port_pair, ProxyServer::Direct(),
                                     kPrivacyModeEnabled);
  SpdySessionKey key_privacy_disabled(host_port_pair, ProxyServer::Direct(),
                                     kPrivacyModeDisabled);

  EXPECT_FALSE(spdy_session_pool_->HasSession(key_privacy_enabled));
  EXPECT_FALSE(spdy_session_pool_->HasSession(key_privacy_disabled));

  // Add SpdySession with PrivacyMode Enabled to the pool.
  scoped_refptr<SpdySession> session_privacy_enabled =
      spdy_session_pool_->Get(key_privacy_enabled, BoundNetLog());

  EXPECT_TRUE(spdy_session_pool_->HasSession(key_privacy_enabled));
  EXPECT_FALSE(spdy_session_pool_->HasSession(key_privacy_disabled));

  // Add SpdySession with PrivacyMode Disabled to the pool.
  scoped_refptr<SpdySession> session_privacy_disabled =
      spdy_session_pool_->Get(key_privacy_disabled, BoundNetLog());

  EXPECT_TRUE(spdy_session_pool_->HasSession(key_privacy_enabled));
  EXPECT_TRUE(spdy_session_pool_->HasSession(key_privacy_disabled));

  spdy_session_pool_->Remove(session_privacy_enabled);
  EXPECT_FALSE(spdy_session_pool_->HasSession(key_privacy_enabled));
  EXPECT_TRUE(spdy_session_pool_->HasSession(key_privacy_disabled));

  spdy_session_pool_->Remove(session_privacy_disabled);
  EXPECT_FALSE(spdy_session_pool_->HasSession(key_privacy_enabled));
  EXPECT_FALSE(spdy_session_pool_->HasSession(key_privacy_disabled));
}

}  // namespace net
