// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_callback.h"
#include "net/base/net_log_unittest.h"
#include "net/base/request_priority.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(ukai): factor out common part with spdy_http_stream_unittest.cc
//
namespace net {

namespace test {

namespace {

const char kStreamUrl[] = "http://www.google.com/";
const char kPostBody[] = "\0hello!\xff";
const size_t kPostBodyLength = arraysize(kPostBody);
const base::StringPiece kPostBodyStringPiece(kPostBody, kPostBodyLength);

class SpdyStreamTest : public ::testing::Test,
                       public ::testing::WithParamInterface<NextProto> {
 protected:
  // A function that takes a SpdyStream and the number of bytes which
  // will unstall the next frame completely.
  typedef base::Callback<void(const base::WeakPtr<SpdyStream>&, int32)>
      UnstallFunction;

  SpdyStreamTest()
      : spdy_util_(GetParam()),
        host_port_pair_("www.google.com", 80),
        session_deps_(GetParam()),
        offset_(0) {}

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

  void RunResumeAfterUnstallRequestResponseTest(
      const UnstallFunction& unstall_function);

  void RunResumeAfterUnstallBidirectionalTest(
      const UnstallFunction& unstall_function);

  // Add{Read,Write}() populates lists that are eventually passed to a
  // SocketData class. |frame| must live for the whole test.

  void AddRead(const SpdyFrame& frame) {
    reads_.push_back(CreateMockRead(frame, offset_++));
  }

  void AddWrite(const SpdyFrame& frame) {
    writes_.push_back(CreateMockWrite(frame, offset_++));
  }

  void AddReadEOF() {
    reads_.push_back(MockRead(ASYNC, 0, offset_++));
  }

  MockRead* GetReads() {
    return vector_as_array(&reads_);
  }

  size_t GetNumReads() const {
    return reads_.size();
  }

  MockWrite* GetWrites() {
    return vector_as_array(&writes_);
  }

  int GetNumWrites() const {
    return writes_.size();
  }

  SpdyTestUtil spdy_util_;
  HostPortPair host_port_pair_;
  SpdySessionDependencies session_deps_;
  scoped_refptr<HttpNetworkSession> session_;

 private:
  // Used by Add{Read,Write}() above.
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  int offset_;
};

INSTANTIATE_TEST_CASE_P(
    NextProto,
    SpdyStreamTest,
    testing::Values(kProtoSPDY2, kProtoSPDY3, kProtoSPDY31, kProtoSPDY4a2));

TEST_P(SpdyStreamTest, SendDataAfterOpen) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  AddRead(*resp);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddWrite(*msg);

  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*echo);

  AddReadEOF();

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ("HTTP/1.1",
            delegate.GetResponseHeaderValue(spdy_util_.GetVersionKey()));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdyStreamTest, PushedStream) {
  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
  scoped_refptr<SpdySession> spdy_session(CreateSpdySession());

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  AddReadEOF();

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  InitializeSpdySession(spdy_session, host_port_pair_);
  BoundNetLog net_log;

  // Conjure up a stream.
  SpdyStream stream(SPDY_PUSH_STREAM,
                    spdy_session.get(),
                    std::string(),
                    DEFAULT_PRIORITY,
                    kSpdyStreamInitialWindowSize,
                    kSpdyStreamInitialWindowSize,
                    net_log);
  stream.set_stream_id(2);
  EXPECT_FALSE(stream.HasUrl());

  // Set a couple of headers.
  SpdyHeaderBlock response;
  spdy_util_.AddUrlToHeaderBlock(kStreamUrl, &response);
  stream.OnInitialResponseHeadersReceived(
      response, base::Time::Now(), base::TimeTicks::Now());

  // Send some basic headers.
  SpdyHeaderBlock headers;
  headers[spdy_util_.GetStatusKey()] = "200";
  headers[spdy_util_.GetVersionKey()] = "OK";
  stream.OnAdditionalResponseHeadersReceived(headers);

  EXPECT_TRUE(stream.HasUrl());
  EXPECT_EQ(kStreamUrl, stream.GetUrl().spec());

  StreamDelegateDoNothing delegate(stream.GetWeakPtr());
  stream.SetDelegate(&delegate);

  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));

  spdy_session->CloseSessionOnError(
      ERR_CONNECTION_CLOSED, true, "Closing session");
}

TEST_P(SpdyStreamTest, StreamError) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> resp(
      spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*resp);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddWrite(*msg);

  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*echo);

  AddReadEOF();

  CapturingBoundNetLog log;

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, log.bound());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  const SpdyStreamId stream_id = delegate.stream_id();

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ("HTTP/1.1",
            delegate.GetResponseHeaderValue(spdy_util_.GetVersionKey()));
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
TEST_P(SpdyStreamTest, SendLargeDataAfterOpenRequestResponse) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  scoped_ptr<SpdyFrame> chunk(
      spdy_util_.ConstructSpdyBodyFrame(
          1, chunk_data.data(), chunk_data.length(), false));
  AddWrite(*chunk);
  AddWrite(*chunk);

  scoped_ptr<SpdyFrame> last_chunk(
      spdy_util_.ConstructSpdyBodyFrame(
          1, chunk_data.data(), chunk_data.length(), true));
  AddWrite(*last_chunk);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  AddRead(*resp);

  AddReadEOF();

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  std::string body_data(3 * kMaxSpdyFrameChunkSize, 'x');
  StreamDelegateWithBody delegate(stream, body_data);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ("HTTP/1.1",
            delegate.GetResponseHeaderValue(spdy_util_.GetVersionKey()));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

// Make sure that large blocks of data are properly split up into
// frame-sized chunks for a bidirectional (i.e., non-HTTP-like)
// stream.
TEST_P(SpdyStreamTest, SendLargeDataAfterOpenBidirectional) {
  GURL url(kStreamUrl);

  session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  AddRead(*resp);

  std::string chunk_data(kMaxSpdyFrameChunkSize, 'x');
  scoped_ptr<SpdyFrame> chunk(
      spdy_util_.ConstructSpdyBodyFrame(
          1, chunk_data.data(), chunk_data.length(), false));
  AddWrite(*chunk);
  AddWrite(*chunk);
  AddWrite(*chunk);

  AddReadEOF();

  OrderedSocketData data(GetReads(), GetNumReads(),
                         GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  std::string body_data(3 * kMaxSpdyFrameChunkSize, 'x');
  StreamDelegateSendImmediate delegate(stream, body_data);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ("HTTP/1.1",
            delegate.GetResponseHeaderValue(spdy_util_.GetVersionKey()));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

// Receiving a header with uppercase ASCII should result in a protocol
// error.
TEST_P(SpdyStreamTest, UpperCaseHeaders) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  const char* const kExtraHeaders[] = {"X-UpperCase", "yes"};
  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(kExtraHeaders, 1, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  // For the initial window update.
  if (spdy_util_.protocol() >= kProtoSPDY31)
    data.RunFor(1);

  data.RunFor(4);

  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, delegate.WaitForClose());
}

// Receiving a header with uppercase ASCII should result in a protocol
// error even for a push stream.
TEST_P(SpdyStreamTest, UpperCaseHeadersOnPush) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  const char* const extra_headers[] = {"X-UpperCase", "yes"};
  scoped_ptr<SpdyFrame>
      push(spdy_util_.ConstructSpdyPush(extra_headers, 1, 2, 1, kStreamUrl));
  AddRead(*push);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  // For the initial window update.
  if (spdy_util_.protocol() >= kProtoSPDY31)
    data.RunFor(1);

  data.RunFor(4);

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.RunFor(1);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// Receiving a header with uppercase ASCII in a HEADERS frame should
// result in a protocol error.
TEST_P(SpdyStreamTest, UpperCaseHeadersInHeadersFrame) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame>
      push(spdy_util_.ConstructSpdyPush(NULL, 0, 2, 1, kStreamUrl));
  AddRead(*push);

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)["X-UpperCase"] = "yes";
  scoped_ptr<SpdyFrame> headers_frame(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));
  AddRead(*headers_frame);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  // For the initial window update.
  if (spdy_util_.protocol() >= kProtoSPDY31)
    data.RunFor(1);

  data.RunFor(3);

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_TRUE(push_stream);

  data.RunFor(1);

  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.RunFor(2);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// Receiving a duplicate header in a HEADERS frame should result in a
// protocol error.
TEST_P(SpdyStreamTest, DuplicateHeaders) {
  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(NULL, 0, false, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame>
      push(spdy_util_.ConstructSpdyPush(NULL, 0, 2, 1, kStreamUrl));
  AddRead(*push);

  scoped_ptr<SpdyHeaderBlock> late_headers(new SpdyHeaderBlock());
  (*late_headers)[spdy_util_.GetStatusKey()] = "500 Server Error";
  scoped_ptr<SpdyFrame> headers_frame(
      spdy_util_.ConstructSpdyControlFrame(late_headers.Pass(),
                                           false,
                                           2,
                                           LOWEST,
                                           HEADERS,
                                           CONTROL_FLAG_NONE,
                                           0));
  AddRead(*headers_frame);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  // For the initial window update.
  if (spdy_util_.protocol() >= kProtoSPDY31)
    data.RunFor(1);

  data.RunFor(3);

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_TRUE(push_stream);

  data.RunFor(1);

  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.RunFor(2);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// The tests below are only for SPDY/3 and above.

// Call IncreaseSendWindowSize on a stream with a large enough delta
// to overflow an int32. The SpdyStream should handle that case
// gracefully.
TEST_P(SpdyStreamTest, IncreaseSendWindowSizeOverflow) {
  if (spdy_util_.protocol() < kProtoSPDY3)
    return;

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  // Triggered by the overflowing call to IncreaseSendWindowSize
  // below.
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_FLOW_CONTROL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  CapturingBoundNetLog log;

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());
  GURL url(kStreamUrl);

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, log.bound());
  ASSERT_TRUE(stream.get() != NULL);
  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  // For the initial window update.
  if (spdy_util_.protocol() >= kProtoSPDY31)
    data.RunFor(1);

  data.RunFor(1);

  int32 old_send_window_size = stream->send_window_size();
  ASSERT_GT(old_send_window_size, 0);
  int32 delta_window_size = kint32max - old_send_window_size + 1;
  stream->IncreaseSendWindowSize(delta_window_size);
  EXPECT_EQ(NULL, stream.get());

  data.RunFor(2);

  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, delegate.WaitForClose());
}

// Functions used with
// RunResumeAfterUnstall{RequestResponse,Bidirectional}Test().

void StallStream(const base::WeakPtr<SpdyStream>& stream) {
  // Reduce the send window size to 0 to stall.
  while (stream->send_window_size() > 0) {
    stream->DecreaseSendWindowSize(
        std::min(kMaxSpdyFrameChunkSize, stream->send_window_size()));
  }
}

void IncreaseStreamSendWindowSize(const base::WeakPtr<SpdyStream>& stream,
                                  int32 delta_window_size) {
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->IncreaseSendWindowSize(delta_window_size);
  EXPECT_FALSE(stream->send_stalled_by_flow_control());
}

void AdjustStreamSendWindowSize(const base::WeakPtr<SpdyStream>& stream,
                                int32 delta_window_size) {
  // Make sure that negative adjustments are handled properly.
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->AdjustSendWindowSize(-delta_window_size);
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->AdjustSendWindowSize(+delta_window_size);
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->AdjustSendWindowSize(+delta_window_size);
  EXPECT_FALSE(stream->send_stalled_by_flow_control());
}

// Given an unstall function, runs a test to make sure that a
// request/response (i.e., an HTTP-like) stream resumes after a stall
// and unstall.
void SpdyStreamTest::RunResumeAfterUnstallRequestResponseTest(
    const UnstallFunction& unstall_function) {
  if (spdy_util_.protocol() < kProtoSPDY3)
    return;

  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> body(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, true));
  AddWrite(*body);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*resp);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateWithBody delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());
  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  StallStream(stream);

  // For the initial window update.
  if (spdy_util_.protocol() >= kProtoSPDY31)
    data.RunFor(1);

  data.RunFor(1);

  EXPECT_TRUE(stream->send_stalled_by_flow_control());

  unstall_function.Run(stream, kPostBodyLength);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  data.RunFor(3);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(":status"));
  EXPECT_EQ("HTTP/1.1", delegate.GetResponseHeaderValue(":version"));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeIncreaseRequestResponse) {
  RunResumeAfterUnstallRequestResponseTest(
      base::Bind(&IncreaseStreamSendWindowSize));
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeAdjustRequestResponse) {
  RunResumeAfterUnstallRequestResponseTest(
      base::Bind(&AdjustStreamSendWindowSize));
}

// Given an unstall function, runs a test to make sure that a
// bidirectional (i.e., non-HTTP-like) stream resumes after a stall
// and unstall.
void SpdyStreamTest::RunResumeAfterUnstallBidirectionalTest(
    const UnstallFunction& unstall_function) {
  if (spdy_util_.protocol() < kProtoSPDY3)
    return;

  GURL url(kStreamUrl);

  session_ =
      SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);

  scoped_ptr<SpdyFrame> initial_window_update(
      spdy_util_.ConstructSpdyWindowUpdate(
          kSessionFlowControlStreamId,
          kDefaultInitialRecvWindowSize - kSpdySessionInitialWindowSize));
  if (spdy_util_.protocol() >= kProtoSPDY31)
    AddWrite(*initial_window_update);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*resp);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddWrite(*msg);

  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*echo);

  AddReadEOF();

  DeterministicSocketData data(GetReads(), GetNumReads(),
                               GetWrites(), GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.deterministic_socket_factory->AddSocketDataProvider(&data);

  scoped_refptr<SpdySession> session(CreateSpdySession());

  InitializeSpdySession(session, host_port_pair_);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrl());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(headers.Pass(), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrl());
  EXPECT_EQ(kStreamUrl, stream->GetUrl().spec());

  data.RunFor(1);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  StallStream(stream);

  // For the initial window update.
  if (spdy_util_.protocol() >= kProtoSPDY31)
    data.RunFor(1);

  data.RunFor(1);

  EXPECT_TRUE(stream->send_stalled_by_flow_control());

  unstall_function.Run(stream, kPostBodyLength);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  data.RunFor(3);

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(":status"));
  EXPECT_EQ("HTTP/1.1", delegate.GetResponseHeaderValue(":version"));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.at_write_eof());
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeIncreaseBidirectional) {
  RunResumeAfterUnstallBidirectionalTest(
      base::Bind(&IncreaseStreamSendWindowSize));
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeAdjustBidirectional) {
  RunResumeAfterUnstallBidirectionalTest(
      base::Bind(&AdjustStreamSendWindowSize));
}

}  // namespace

}  // namespace test

}  // namespace net
