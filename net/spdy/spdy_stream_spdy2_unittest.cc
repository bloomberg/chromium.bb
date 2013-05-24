// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log_unittest.h"
#include "net/base/request_priority.h"
#include "net/socket/next_proto.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "net/spdy/spdy_test_util_spdy2.h"
#include "net/spdy/spdy_websocket_test_util_spdy2.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace net::test_spdy2;

// TODO(ukai): factor out common part with spdy_http_stream_unittest.cc
//
namespace net {

namespace test {

namespace {

const char kStreamUrl[] = "http://www.google.com/";
const char kPostBody[] = "\0hello!\xff";
const size_t kPostBodyLength = arraysize(kPostBody);
const base::StringPiece kPostBodyStringPiece(kPostBody, kPostBodyLength);

class SpdyStreamSpdy2Test : public testing::Test {
 protected:
  SpdyStreamSpdy2Test()
      : spdy_util_(kProtoSPDY2),
        host_port_pair_("www.google.com", 80),
        session_deps_(kProtoSPDY2) {}

  scoped_refptr<SpdySession> CreateSpdySession() {
    SpdySessionKey key(host_port_pair_, ProxyServer::Direct(),
                       kPrivacyModeDisabled);
    scoped_refptr<SpdySession> session(
        session_->spdy_session_pool()->Get(key, BoundNetLog()));
    return session;
  }

  void InitializeSpdySession(const scoped_refptr<SpdySession>& session,
                             const HostPortPair& host_port_pair) {
    scoped_refptr<TransportSocketParams> transport_params(
        new TransportSocketParams(host_port_pair, LOWEST, false, false,
                                  OnHostResolutionCallback()));

    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    EXPECT_EQ(OK, connection->Init(host_port_pair_.ToString(), transport_params,
                                   LOWEST, CompletionCallback(),
                                   session_->GetTransportSocketPool(
                                       HttpNetworkSession::NORMAL_SOCKET_POOL),
                                   BoundNetLog()));
    session->InitializeWithSocket(connection.release(), false, OK);
  }

  virtual void TearDown() {
    base::MessageLoop::current()->RunUntilIdle();
  }

  SpdyTestUtil spdy_util_;
  HostPortPair host_port_pair_;
  SpdySessionDependencies session_deps_;
  scoped_refptr<HttpNetworkSession> session_;
};

TEST_F(SpdyStreamSpdy2Test, SendDataAfterOpen) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      ConstructSpdyPost(kStreamUrl, kPostBodyLength, NULL, 0));
  scoped_ptr<SpdyFrame> msg(
      ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*msg),
  };
  writes[0].sequence_number = 0;
  writes[1].sequence_number = 2;

  scoped_ptr<SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  scoped_ptr<SpdyFrame> echo(
      ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*echo),
    MockRead(ASYNC, 0, 0), // EOF
  };
  reads[0].sequence_number = 1;
  reads[1].sequence_number = 3;
  reads[2].sequence_number = 4;

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(headers.Pass(), true));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue("status"));
  EXPECT_EQ("HTTP/1.1", delegate.GetResponseHeaderValue("version"));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_F(SpdyStreamSpdy2Test, PushedStream) {
  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  scoped_refptr<SpdySession> spdy_session(CreateSpdySession());

  MockRead reads[] = {
    MockRead(ASYNC, 0, 0), // EOF
  };

  OrderedSocketData data(reads, arraysize(reads), NULL, 0);
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  InitializeSpdySession(spdy_session, host_port_pair_);
  BoundNetLog net_log;

  // Conjure up a stream.
  SpdyStream stream(spdy_session,
                    std::string(),
                    DEFAULT_PRIORITY,
                    kSpdyStreamInitialWindowSize,
                    kSpdyStreamInitialWindowSize,
                    true,
                    net_log);
  stream.set_stream_id(2);
  EXPECT_FALSE(stream.response_received());
  EXPECT_FALSE(stream.HasUrl());

  // Set a couple of headers.
  SpdyHeaderBlock response;
  response["url"] = kStreamUrl;
  stream.OnResponseReceived(response);

  // Send some basic headers.
  SpdyHeaderBlock headers;
  response["status"] = "200";
  response["version"] = "OK";
  stream.OnHeaders(headers);

  stream.set_response_received();
  EXPECT_TRUE(stream.response_received());
  EXPECT_TRUE(stream.HasUrl());
  EXPECT_EQ(kStreamUrl, stream.GetUrl().spec());
}

TEST_F(SpdyStreamSpdy2Test, StreamError) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      ConstructSpdyPost(kStreamUrl, kPostBodyLength, NULL, 0));
  scoped_ptr<SpdyFrame> msg(
      ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  MockWrite writes[] = {
    CreateMockWrite(*req),
    CreateMockWrite(*msg),
  };
  writes[0].sequence_number = 0;
  writes[1].sequence_number = 2;

  scoped_ptr<SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> echo(
      ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  MockRead reads[] = {
    CreateMockRead(*resp),
    CreateMockRead(*echo),
    MockRead(ASYNC, 0, 0),  // EOF
  };
  reads[0].sequence_number = 1;
  reads[1].sequence_number = 3;
  reads[2].sequence_number = 4;

  CapturingBoundNetLog log;

  OrderedSocketData data(reads, arraysize(reads),
                         writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(session, url, LOWEST, log.bound());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(headers.Pass(), true));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  const SpdyStreamId stream_id = delegate.stream_id();

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue("status"));
  EXPECT_EQ("HTTP/1.1", delegate.GetResponseHeaderValue("version"));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());

  // Check that the NetLog was filled reasonably.
  net::CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged SPDY_STREAM_ERROR correctly.
  int pos = net::ExpectLogContainsSomewhere(
      entries, 0,
      net::NetLog::TYPE_SPDY_STREAM_ERROR,
      net::NetLog::PHASE_NONE);

  int stream_id2;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &stream_id2));
  EXPECT_EQ(static_cast<int>(stream_id), stream_id2);
}

// Make sure that large blocks of data are properly split up into
// frame-sized chunks for a request/response (i.e., an HTTP-like)
// stream.
TEST_F(SpdyStreamSpdy2Test, SendLargeDataAfterOpenRequestResponse) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      ConstructSpdyPost(kStreamUrl, kPostBodyLength, NULL, 0));
  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  scoped_ptr<SpdyFrame> chunk(
      ConstructSpdyBodyFrame(
          1, chunk_data.data(), chunk_data.length(), false));
  MockWrite writes[] = {
    CreateMockWrite(*req, 0),
    CreateMockWrite(*chunk, 1),
    CreateMockWrite(*chunk, 2),
    CreateMockWrite(*chunk, 3),
  };

  scoped_ptr<SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp, 4),
    MockRead(ASYNC, 0, 0, 5), // EOF
  };

  OrderedSocketData data(reads, arraysize(reads), writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  std::string body_data(3 * kMaxSpdyFrameChunkSize, 'x');
  StreamDelegateWithBody delegate(stream, body_data);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(headers.Pass(), true));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue("status"));
  EXPECT_EQ("HTTP/1.1", delegate.GetResponseHeaderValue("version"));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

// Make sure that large blocks of data are properly split up into
// frame-sized chunks for a bidirectional (i.e., non-HTTP-like)
// stream.
TEST_F(SpdyStreamSpdy2Test, SendLargeDataAfterOpenBidirectional) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> req(
      ConstructSpdyPost(kStreamUrl, kPostBodyLength, NULL, 0));
  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  scoped_ptr<SpdyFrame> chunk(
      ConstructSpdyBodyFrame(
          1, chunk_data.data(), chunk_data.length(), false));
  MockWrite writes[] = {
    CreateMockWrite(*req, 0),
    CreateMockWrite(*chunk, 2),
    CreateMockWrite(*chunk, 3),
    CreateMockWrite(*chunk, 4),
  };

  scoped_ptr<SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    MockRead(ASYNC, 0, 0, 5), // EOF
  };

  OrderedSocketData data(reads, arraysize(reads), writes, arraysize(writes));
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  std::string body_data(3 * kMaxSpdyFrameChunkSize, 'x');
  StreamDelegateSendImmediate delegate(stream, body_data);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequest(headers.Pass(), true));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue("status"));
  EXPECT_EQ("HTTP/1.1", delegate.GetResponseHeaderValue("version"));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

}  // namespace

}  // namespace test

}  // namespace net
