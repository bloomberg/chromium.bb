// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include <stdint.h>
#include <cstddef>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_entry.h"
#include "net/log/test_net_log_util.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_test_util.h"
#include "net/spdy/buffered_spdy_framer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_protocol.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_stream_test_util.h"
#include "net/spdy/spdy_test_util_common.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(ukai): factor out common part with spdy_http_stream_unittest.cc
//
namespace net {

namespace test {

namespace {

enum TestCase {
  // Test using the SPDY/3.1 protocol.
  kTestCaseSPDY31,

  // Test using the HTTP/2 protocol, without specifying a stream
  // dependency based on the RequestPriority.
  kTestCaseHTTP2NoPriorityDependencies,

  // Test using the HTTP/2 protocol, specifying a stream
  // dependency based on the RequestPriority.
  kTestCaseHTTP2PriorityDependencies
};

const char kStreamUrl[] = "http://www.example.org/";
const char kPostBody[] = "\0hello!\xff";
const size_t kPostBodyLength = arraysize(kPostBody);
const base::StringPiece kPostBodyStringPiece(kPostBody, kPostBodyLength);

}  // namespace

class SpdyStreamTest : public ::testing::Test,
                       public ::testing::WithParamInterface<TestCase> {
 protected:
  // A function that takes a SpdyStream and the number of bytes which
  // will unstall the next frame completely.
  typedef base::Callback<void(const base::WeakPtr<SpdyStream>&, int32_t)>
      UnstallFunction;

  SpdyStreamTest()
      : spdy_util_(GetProtocol(), GetDependenciesFromPriority()),
        session_deps_(GetProtocol()),
        session_(SpdySessionDependencies::SpdyCreateSession(&session_deps_)),
        offset_(0) {
    SpdySession::SetPriorityDependencyDefaultForTesting(
        GetDependenciesFromPriority());
  }

  ~SpdyStreamTest() {
    SpdySession::SetPriorityDependencyDefaultForTesting(false);
  }

  base::WeakPtr<SpdySession> CreateDefaultSpdySession() {
    SpdySessionKey key(HostPortPair("www.example.org", 80),
                       ProxyServer::Direct(), PRIVACY_MODE_DISABLED);
    return CreateInsecureSpdySession(session_.get(), key, BoundNetLog());
  }

  void TearDown() override { base::MessageLoop::current()->RunUntilIdle(); }

  NextProto GetProtocol() const {
    return GetParam() == kTestCaseSPDY31 ? kProtoSPDY31 : kProtoHTTP2;
  }

  bool GetDependenciesFromPriority() const {
    return GetParam() == kTestCaseHTTP2PriorityDependencies;
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

  void AddWritePause() {
    writes_.push_back(MockWrite(ASYNC, ERR_IO_PENDING, offset_++));
  }

  void AddReadPause() {
    reads_.push_back(MockRead(ASYNC, ERR_IO_PENDING, offset_++));
  }

  MockRead* GetReads() { return reads_.data(); }

  size_t GetNumReads() const {
    return reads_.size();
  }

  MockWrite* GetWrites() { return writes_.data(); }

  int GetNumWrites() const {
    return writes_.size();
  }

  SpdyTestUtil spdy_util_;
  SpdySessionDependencies session_deps_;
  scoped_ptr<HttpNetworkSession> session_;

 private:
  // Used by Add{Read,Write}() above.
  std::vector<MockWrite> writes_;
  std::vector<MockRead> reads_;
  int offset_;
};

INSTANTIATE_TEST_CASE_P(ProtoPlusDepend,
                        SpdyStreamTest,
                        testing::Values(kTestCaseSPDY31,
                                        kTestCaseHTTP2NoPriorityDependencies,
                                        kTestCaseHTTP2PriorityDependencies));

TEST_P(SpdyStreamTest, SendDataAfterOpen) {
  GURL url(kStreamUrl);

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

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Delegate that receives trailers.
class StreamDelegateWithTrailers : public test::StreamDelegateWithBody {
 public:
  StreamDelegateWithTrailers(const base::WeakPtr<SpdyStream>& stream,
                             base::StringPiece data)
      : StreamDelegateWithBody(stream, data) {}

  ~StreamDelegateWithTrailers() override {}

  void OnTrailers(const SpdyHeaderBlock& trailers) override {
    trailers_ = trailers;
  }

  const SpdyHeaderBlock& trailers() const { return trailers_; }

 private:
  SpdyHeaderBlock trailers_;
};

// Regression test for crbug.com/481033.
TEST_P(SpdyStreamTest, Trailers) {
  GURL url(kStreamUrl);

  scoped_ptr<SpdyFrame> req(spdy_util_.ConstructSpdyPost(
      kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, true));
  AddWrite(*msg);

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyPostSynReply(NULL, 0));
  AddRead(*resp);

  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*echo);

  SpdyHeaderBlock late_headers;
  late_headers["foo"] = "bar";
  scoped_ptr<SpdyFrame> trailers(
      spdy_util_.ConstructSpdyResponseHeaders(1, late_headers, false));
  AddRead(*trailers);

  AddReadEOF();

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream = CreateStreamSynchronously(
      SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateWithTrailers delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  const SpdyHeaderBlock& received_trailers = delegate.trailers();
  SpdyHeaderBlock::const_iterator it = received_trailers.find("foo");
  EXPECT_EQ("bar", it->second);
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_P(SpdyStreamTest, PushedStream) {
  AddReadEOF();

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> spdy_session(CreateDefaultSpdySession());

  // Conjure up a stream.
  SpdyStream stream(SPDY_PUSH_STREAM, spdy_session, GURL(), DEFAULT_PRIORITY,
                    SpdySession::GetDefaultInitialWindowSize(kProtoSPDY31),
                    SpdySession::GetDefaultInitialWindowSize(kProtoSPDY31),
                    BoundNetLog());
  stream.set_stream_id(2);
  EXPECT_FALSE(stream.HasUrlFromHeaders());

  // Set required request headers.
  SpdyHeaderBlock request_headers;
  spdy_util_.AddUrlToHeaderBlock(kStreamUrl, &request_headers);
  stream.OnPushPromiseHeadersReceived(request_headers);

  // Send some basic response headers.
  SpdyHeaderBlock response;
  response[spdy_util_.GetStatusKey()] = "200";
  response[spdy_util_.GetVersionKey()] = "OK";
  stream.OnInitialResponseHeadersReceived(
      response, base::Time::Now(), base::TimeTicks::Now());

  // And some more headers.
  // TODO(baranovich): not valid for HTTP 2.
  SpdyHeaderBlock headers;
  headers["alpha"] = "beta";
  stream.OnAdditionalResponseHeadersReceived(headers);

  EXPECT_TRUE(stream.HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream.GetUrlFromHeaders().spec());

  StreamDelegateDoNothing delegate(stream.GetWeakPtr());
  stream.SetDelegate(&delegate);

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ("beta", delegate.GetResponseHeaderValue("alpha"));

  EXPECT_TRUE(spdy_session == NULL);
}

TEST_P(SpdyStreamTest, StreamError) {
  GURL url(kStreamUrl);

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

  BoundTestNetLog log;

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, log.bound());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  const SpdyStreamId stream_id = delegate.stream_id();

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.AllWriteDataConsumed());

  // Check that the NetLog was filled reasonably.
  TestNetLogEntry::List entries;
  log.GetEntries(&entries);
  EXPECT_LT(0u, entries.size());

  // Check that we logged SPDY_STREAM_ERROR correctly.
  int pos = ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_HTTP2_STREAM_ERROR, NetLog::PHASE_NONE);

  int stream_id2;
  ASSERT_TRUE(entries[pos].GetIntegerValue("stream_id", &stream_id2));
  EXPECT_EQ(static_cast<int>(stream_id), stream_id2);
}

// Make sure that large blocks of data are properly split up into
// frame-sized chunks for a request/response (i.e., an HTTP-like)
// stream.
TEST_P(SpdyStreamTest, SendLargeDataAfterOpenRequestResponse) {
  GURL url(kStreamUrl);

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

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  std::string body_data(3 * kMaxSpdyFrameChunkSize, 'x');
  StreamDelegateWithBody delegate(stream, body_data);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Make sure that large blocks of data are properly split up into
// frame-sized chunks for a bidirectional (i.e., non-HTTP-like)
// stream.
TEST_P(SpdyStreamTest, SendLargeDataAfterOpenBidirectional) {
  GURL url(kStreamUrl);

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

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  std::string body_data(3 * kMaxSpdyFrameChunkSize, 'x');
  StreamDelegateSendImmediate delegate(stream, body_data);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(spdy_util_.GetStatusKey()));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

// Receiving a header with uppercase ASCII should result in a protocol
// error.
TEST_P(SpdyStreamTest, UpperCaseHeaders) {
  GURL url(kStreamUrl);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST, true));
  AddWrite(*syn);

  const char* const kExtraHeaders[] = {"X-UpperCase", "yes"};
  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(kExtraHeaders, 1, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequestHeaders(std::move(headers),
                                                       NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  EXPECT_EQ(ERR_SPDY_PROTOCOL_ERROR, delegate.WaitForClose());
}

// Receiving a header with uppercase ASCII should result in a protocol
// error even for a push stream.
TEST_P(SpdyStreamTest, UpperCaseHeadersOnPush) {
  GURL url(kStreamUrl);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST, true));
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

  AddReadPause();

  AddReadEOF();

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequestHeaders(std::move(headers),
                                                       NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunUntilPaused();

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.Resume();

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// Receiving a header with uppercase ASCII in a HEADERS frame should
// result in a protocol error.
TEST_P(SpdyStreamTest, UpperCaseHeadersInHeadersFrame) {
  GURL url(kStreamUrl);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame>
      push(spdy_util_.ConstructSpdyPush(NULL, 0, 2, 1, kStreamUrl));
  AddRead(*push);

  AddReadPause();

  SpdyHeaderBlock late_headers;
  late_headers["X-UpperCase"] = "yes";
  scoped_ptr<SpdyFrame> headers_frame(
      spdy_util_.ConstructSpdyReply(2, late_headers));
  AddRead(*headers_frame);

  AddWritePause();

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequestHeaders(std::move(headers),
                                                       NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunUntilPaused();

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_TRUE(push_stream);

  data.Resume();
  data.RunUntilPaused();

  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.Resume();

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// Receiving a duplicate header in a HEADERS frame should result in a
// protocol error.
TEST_P(SpdyStreamTest, DuplicateHeaders) {
  GURL url(kStreamUrl);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST, true));
  AddWrite(*syn);

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  scoped_ptr<SpdyFrame>
      push(spdy_util_.ConstructSpdyPush(NULL, 0, 2, 1, kStreamUrl));
  AddRead(*push);

  AddReadPause();

  SpdyHeaderBlock late_headers;
  late_headers[spdy_util_.GetStatusKey()] = "500 Server Error";
  scoped_ptr<SpdyFrame> headers_frame(
      spdy_util_.ConstructSpdyReply(2, late_headers));
  AddRead(*headers_frame);

  AddReadPause();

  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(2, RST_STREAM_PROTOCOL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequestHeaders(std::move(headers),
                                                       NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunUntilPaused();

  base::WeakPtr<SpdyStream> push_stream;
  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_TRUE(push_stream);

  data.Resume();
  data.RunUntilPaused();

  EXPECT_EQ(OK, session->GetPushStream(url, &push_stream, BoundNetLog()));
  EXPECT_FALSE(push_stream);

  data.Resume();

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

// Call IncreaseSendWindowSize on a stream with a large enough delta
// to overflow an int32_t. The SpdyStream should handle that case
// gracefully.
TEST_P(SpdyStreamTest, IncreaseSendWindowSizeOverflow) {
  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  AddReadPause();

  // Triggered by the overflowing call to IncreaseSendWindowSize
  // below.
  scoped_ptr<SpdyFrame> rst(
      spdy_util_.ConstructSpdyRstStream(1, RST_STREAM_FLOW_CONTROL_ERROR));
  AddWrite(*rst);

  AddReadEOF();

  BoundTestNetLog log;

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());
  GURL url(kStreamUrl);

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, log.bound());
  ASSERT_TRUE(stream.get() != NULL);
  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunUntilPaused();

  int32_t old_send_window_size = stream->send_window_size();
  ASSERT_GT(old_send_window_size, 0);
  int32_t delta_window_size =
      std::numeric_limits<int32_t>::max() - old_send_window_size + 1;
  stream->IncreaseSendWindowSize(delta_window_size);
  EXPECT_EQ(NULL, stream.get());

  data.Resume();
  base::RunLoop().RunUntilIdle();

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
                                  int32_t delta_window_size) {
  EXPECT_TRUE(stream->send_stalled_by_flow_control());
  stream->IncreaseSendWindowSize(delta_window_size);
  EXPECT_FALSE(stream->send_stalled_by_flow_control());
}

void AdjustStreamSendWindowSize(const base::WeakPtr<SpdyStream>& stream,
                                int32_t delta_window_size) {
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
  GURL url(kStreamUrl);

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

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateWithBody delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());
  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  StallStream(stream);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream->send_stalled_by_flow_control());

  unstall_function.Run(stream, kPostBodyLength);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(), delegate.TakeReceivedData());
  EXPECT_TRUE(data.AllWriteDataConsumed());
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
  GURL url(kStreamUrl);

  scoped_ptr<SpdyFrame> req(
      spdy_util_.ConstructSpdyPost(
          kStreamUrl, 1, kPostBodyLength, LOWEST, NULL, 0));
  AddWrite(*req);

  AddReadPause();

  scoped_ptr<SpdyFrame> resp(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*resp);

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddWrite(*msg);

  scoped_ptr<SpdyFrame> echo(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*echo);

  AddReadEOF();

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_BIDIRECTIONAL_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateSendImmediate delegate(stream, kPostBodyStringPiece);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructPostHeaderBlock(kStreamUrl, kPostBodyLength));
  EXPECT_EQ(ERR_IO_PENDING,
            stream->SendRequestHeaders(std::move(headers), MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  data.RunUntilPaused();

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  StallStream(stream);

  data.Resume();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(stream->send_stalled_by_flow_control());

  unstall_function.Run(stream, kPostBodyLength);

  EXPECT_FALSE(stream->send_stalled_by_flow_control());

  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());

  EXPECT_TRUE(delegate.send_headers_completed());
  EXPECT_EQ("200", delegate.GetResponseHeaderValue(":status"));
  EXPECT_EQ(std::string(kPostBody, kPostBodyLength),
            delegate.TakeReceivedData());
  EXPECT_TRUE(data.AllWriteDataConsumed());
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeIncreaseBidirectional) {
  RunResumeAfterUnstallBidirectionalTest(
      base::Bind(&IncreaseStreamSendWindowSize));
}

TEST_P(SpdyStreamTest, ResumeAfterSendWindowSizeAdjustBidirectional) {
  RunResumeAfterUnstallBidirectionalTest(
      base::Bind(&AdjustStreamSendWindowSize));
}

// Test calculation of amount of bytes received from network.
TEST_P(SpdyStreamTest, ReceivedBytes) {
  GURL url(kStreamUrl);

  scoped_ptr<SpdyFrame> syn(
      spdy_util_.ConstructSpdyGet(nullptr, 0, 1, LOWEST, true));
  AddWrite(*syn);

  AddReadPause();

  scoped_ptr<SpdyFrame>
      reply(spdy_util_.ConstructSpdyGetSynReply(NULL, 0, 1));
  AddRead(*reply);

  AddReadPause();

  scoped_ptr<SpdyFrame> msg(
      spdy_util_.ConstructSpdyBodyFrame(1, kPostBody, kPostBodyLength, false));
  AddRead(*msg);

  AddReadPause();

  AddReadEOF();

  SequencedSocketData data(GetReads(), GetNumReads(), GetWrites(),
                           GetNumWrites());
  MockConnect connect_data(SYNCHRONOUS, OK);
  data.set_connect_data(connect_data);

  session_deps_.socket_factory->AddSocketDataProvider(&data);

  base::WeakPtr<SpdySession> session(CreateDefaultSpdySession());

  base::WeakPtr<SpdyStream> stream =
      CreateStreamSynchronously(
          SPDY_REQUEST_RESPONSE_STREAM, session, url, LOWEST, BoundNetLog());
  ASSERT_TRUE(stream.get() != NULL);

  StreamDelegateDoNothing delegate(stream);
  stream->SetDelegate(&delegate);

  EXPECT_FALSE(stream->HasUrlFromHeaders());

  scoped_ptr<SpdyHeaderBlock> headers(
      spdy_util_.ConstructGetHeaderBlock(kStreamUrl));
  EXPECT_EQ(ERR_IO_PENDING, stream->SendRequestHeaders(std::move(headers),
                                                       NO_MORE_DATA_TO_SEND));
  EXPECT_TRUE(stream->HasUrlFromHeaders());
  EXPECT_EQ(kStreamUrl, stream->GetUrlFromHeaders().spec());

  int64_t reply_frame_len = reply->size();
  int64_t data_header_len = SpdyConstants::GetDataFrameMinimumSize(
      NextProtoToSpdyMajorVersion(GetProtocol()));
  int64_t data_frame_len = data_header_len + kPostBodyLength;
  int64_t response_len = reply_frame_len + data_frame_len;

  EXPECT_EQ(0, stream->raw_received_bytes());

  // SYN
  data.RunUntilPaused();
  EXPECT_EQ(0, stream->raw_received_bytes());

  // REPLY
  data.Resume();
  data.RunUntilPaused();
  EXPECT_EQ(reply_frame_len, stream->raw_received_bytes());

  // DATA
  data.Resume();
  data.RunUntilPaused();
  EXPECT_EQ(response_len, stream->raw_received_bytes());

  // FIN
  data.Resume();
  EXPECT_EQ(ERR_CONNECTION_CLOSED, delegate.WaitForClose());
}

}  // namespace test

}  // namespace net
