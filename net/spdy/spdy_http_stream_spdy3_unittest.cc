// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_http_stream.h"

#include "base/memory/scoped_ptr.h"
#include "base/threading/sequenced_worker_pool.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/signature_creator.h"
#include "net/base/capturing_net_log.h"
#include "net/base/load_timing_info.h"
#include "net/base/load_timing_info_test_util.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_element_reader.h"
#include "net/cert/asn1_util.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/spdy/spdy_credential_builder.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_test_util_spdy3.h"
#include "net/ssl/default_server_bound_cert_store.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace net::test_spdy3;

namespace net {

namespace {

// Tests the load timing of a stream that's connected and is not the first
// request sent on a connection.
void TestLoadTimingReused(const HttpStream& stream) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(stream.GetLoadTimingInfo(&load_timing_info));

  EXPECT_TRUE(load_timing_info.socket_reused);
  EXPECT_NE(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasNoTimes(load_timing_info.connect_timing);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

// Tests the load timing of a stream that's connected and using a fresh
// connection.
void TestLoadTimingNotReused(const HttpStream& stream) {
  LoadTimingInfo load_timing_info;
  EXPECT_TRUE(stream.GetLoadTimingInfo(&load_timing_info));

  EXPECT_FALSE(load_timing_info.socket_reused);
  EXPECT_NE(NetLog::Source::kInvalidId, load_timing_info.socket_log_id);

  ExpectConnectTimingHasTimes(load_timing_info.connect_timing,
                              CONNECT_TIMING_HAS_DNS_TIMES);
  ExpectLoadTimingHasOnlyConnectionTimes(load_timing_info);
}

}  // namespace

class SpdyHttpStreamSpdy3Test : public testing::Test {
 public:
  SpdyHttpStreamSpdy3Test() {
    session_deps_.net_log = &net_log_;
  }

  DeterministicSocketData* deterministic_data() {
    return deterministic_data_.get();
  }

  OrderedSocketData* data() { return data_.get(); }

 protected:
  virtual void TearDown() OVERRIDE {
    MessageLoop::current()->RunUntilIdle();
  }

  // Initializes the session using DeterministicSocketData.  It's advisable
  // to use this function rather than the OrderedSocketData, since the
  // DeterministicSocketData behaves in a reasonable manner.
  int InitSessionDeterministic(MockRead* reads, size_t reads_count,
                               MockWrite* writes, size_t writes_count,
                               HostPortPair& host_port_pair) {
    HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
    deterministic_data_.reset(
        new DeterministicSocketData(reads, reads_count, writes, writes_count));
    session_deps_.deterministic_socket_factory->AddSocketDataProvider(
        deterministic_data_.get());
    http_session_ =
        SpdySessionDependencies::SpdyCreateSessionDeterministic(&session_deps_);
    session_ = http_session_->spdy_session_pool()->Get(pair, BoundNetLog());
    transport_params_ = new TransportSocketParams(host_port_pair,
                                                  MEDIUM, false, false,
                                                  OnHostResolutionCallback());
    TestCompletionCallback callback;
    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    EXPECT_EQ(ERR_IO_PENDING,
              connection->Init(host_port_pair.ToString(),
                               transport_params_,
                               MEDIUM,
                               callback.callback(),
                               http_session_->GetTransportSocketPool(
                                   HttpNetworkSession::NORMAL_SOCKET_POOL),
                               BoundNetLog()));
    EXPECT_EQ(OK, callback.WaitForResult());
    return session_->InitializeWithSocket(connection.release(), false, OK);
  }

  // Initializes the session using the finicky OrderedSocketData class.
  int InitSession(MockRead* reads, size_t reads_count,
                  MockWrite* writes, size_t writes_count,
                  HostPortPair& host_port_pair) {
    HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
    data_.reset(new OrderedSocketData(reads, reads_count,
                                      writes, writes_count));
    session_deps_.socket_factory->AddSocketDataProvider(data_.get());
    http_session_ = SpdySessionDependencies::SpdyCreateSession(&session_deps_);
    session_ = http_session_->spdy_session_pool()->Get(pair, BoundNetLog());
    transport_params_ = new TransportSocketParams(host_port_pair,
                                                  MEDIUM, false, false,
                                                  OnHostResolutionCallback());
    TestCompletionCallback callback;
    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    EXPECT_EQ(ERR_IO_PENDING,
              connection->Init(host_port_pair.ToString(),
                               transport_params_,
                               MEDIUM,
                               callback.callback(),
                               http_session_->GetTransportSocketPool(
                                   HttpNetworkSession::NORMAL_SOCKET_POOL),
                               BoundNetLog()));
    EXPECT_EQ(OK, callback.WaitForResult());
    return session_->InitializeWithSocket(connection.release(), false, OK);
  }

  void TestSendCredentials(
    ServerBoundCertService* server_bound_cert_service,
    const std::string& cert,
    const std::string& proof);

  CapturingNetLog net_log_;
  SpdySessionDependencies session_deps_;
  scoped_ptr<OrderedSocketData> data_;
  scoped_ptr<DeterministicSocketData> deterministic_data_;
  scoped_refptr<HttpNetworkSession> http_session_;
  scoped_refptr<SpdySession> session_;
  scoped_refptr<TransportSocketParams> transport_params_;

 private:
  MockECSignatureCreatorFactory ec_signature_creator_factory_;
};

TEST_F(SpdyHttpStreamSpdy3Test, SendRequest) {
  scoped_ptr<SpdyFrame> req(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(SYNCHRONOUS, 0, 3)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
  EXPECT_EQ(OK, InitSession(reads, arraysize(reads), writes, arraysize(writes),
      host_port_pair));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(
      new SpdyHttpStream(session_.get(), true));
  // Make sure getting load timing information the stream early does not crash.
  LoadTimingInfo load_timing_info;
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(&request, DEFAULT_PRIORITY,
                                    net_log, CompletionCallback()));
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_FALSE(http_stream->GetLoadTimingInfo(&load_timing_info));

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // Can get timing information once the stream connects.
  TestLoadTimingNotReused(*http_stream);

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());

  TestLoadTimingNotReused(*http_stream);
  http_stream->Close(true);
  // Test that there's no crash when trying to get the load timing after the
  // stream has been closed.
  TestLoadTimingNotReused(*http_stream);
}

TEST_F(SpdyHttpStreamSpdy3Test, LoadTimingTwoRequests) {
  scoped_ptr<SpdyFrame> req1(ConstructSpdyGet(NULL, 0, false, 1, LOWEST));
  scoped_ptr<SpdyFrame> req2(ConstructSpdyGet(NULL, 0, false, 3, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req1, 0),
    CreateMockWrite(*req2, 1),
  };
  scoped_ptr<SpdyFrame> resp1(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> body1(ConstructSpdyBodyFrame(1, "", 0, true));
  scoped_ptr<SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  scoped_ptr<SpdyFrame> body2(ConstructSpdyBodyFrame(3, "", 0, true));
  MockRead reads[] = {
    CreateMockRead(*resp1, 2),
    CreateMockRead(*body1, 3),
    CreateMockRead(*resp2, 4),
    CreateMockRead(*body2, 5),
    MockRead(ASYNC, 0, 6)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
  ASSERT_EQ(OK, InitSessionDeterministic(reads, arraysize(reads),
                                         writes, arraysize(writes),
                                         host_port_pair));

  HttpRequestInfo request1;
  request1.method = "GET";
  request1.url = GURL("http://www.google.com/");
  TestCompletionCallback callback1;
  HttpResponseInfo response1;
  HttpRequestHeaders headers1;
  scoped_ptr<SpdyHttpStream> http_stream1(
      new SpdyHttpStream(session_.get(), true));

  ASSERT_EQ(OK,
            http_stream1->InitializeStream(&request1, DEFAULT_PRIORITY,
                                           BoundNetLog(),
                                           CompletionCallback()));
  EXPECT_EQ(ERR_IO_PENDING, http_stream1->SendRequest(headers1, &response1,
                                                      callback1.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  HttpRequestInfo request2;
  request2.method = "GET";
  request2.url = GURL("http://www.google.com/");
  TestCompletionCallback callback2;
  HttpResponseInfo response2;
  HttpRequestHeaders headers2;
  scoped_ptr<SpdyHttpStream> http_stream2(
      new SpdyHttpStream(session_.get(), true));

  ASSERT_EQ(OK,
            http_stream2->InitializeStream(&request2, DEFAULT_PRIORITY,
                                           BoundNetLog(),
                                           CompletionCallback()));
  EXPECT_EQ(ERR_IO_PENDING, http_stream2->SendRequest(headers2, &response2,
                                                      callback2.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  // First write.
  deterministic_data()->RunFor(1);
  EXPECT_LE(0, callback1.WaitForResult());

  TestLoadTimingNotReused(*http_stream1);
  LoadTimingInfo load_timing_info1;
  LoadTimingInfo load_timing_info2;
  EXPECT_TRUE(http_stream1->GetLoadTimingInfo(&load_timing_info1));
  EXPECT_FALSE(http_stream2->GetLoadTimingInfo(&load_timing_info2));

  // Second write.
  deterministic_data()->RunFor(1);
  EXPECT_LE(0, callback2.WaitForResult());
  TestLoadTimingReused(*http_stream2);
  EXPECT_TRUE(http_stream2->GetLoadTimingInfo(&load_timing_info2));
  EXPECT_EQ(load_timing_info1.socket_log_id, load_timing_info2.socket_log_id);

  // All the reads.
  deterministic_data()->RunFor(6);

  // Read stream 1 to completion, before making sure we can still read load
  // timing from both streams.
  scoped_refptr<IOBuffer> buf1(new IOBuffer(1));
  ASSERT_EQ(0, http_stream1->ReadResponseBody(buf1, 1, callback1.callback()));

  // Stream 1 has been read to completion.
  TestLoadTimingNotReused(*http_stream1);
  // Stream 2 still has queued body data.
  TestLoadTimingReused(*http_stream2);
}

TEST_F(SpdyHttpStreamSpdy3Test, SendChunkedPost) {
  scoped_ptr<SpdyFrame> req(ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> body(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0),
    CreateMockWrite(*body, 1),  // POST upload frame
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    CreateMockRead(*body, 3),
    MockRead(SYNCHRONOUS, 0, 4)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
  EXPECT_EQ(OK, InitSession(reads, arraysize(reads), writes, arraysize(writes),
                            host_port_pair));

  UploadDataStream upload_stream(UploadDataStream::CHUNKED, 0);
  const int kFirstChunkSize = kUploadDataSize/2;
  upload_stream.AppendChunk(kUploadData, kFirstChunkSize, false);
  upload_stream.AppendChunk(kUploadData + kFirstChunkSize,
                            kUploadDataSize - kFirstChunkSize, true);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_stream;

  ASSERT_EQ(OK, upload_stream.Init(CompletionCallback()));

  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  SpdyHttpStream http_stream(session_.get(), true);
  ASSERT_EQ(
      OK,
      http_stream.InitializeStream(&request, DEFAULT_PRIORITY,
                                   net_log, CompletionCallback()));

  EXPECT_EQ(ERR_IO_PENDING, http_stream.SendRequest(
      headers, &response, callback.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  // This results in writing the post body and reading the response headers.
  callback.WaitForResult();

  // This triggers reading the body and the EOF, causing the session to shut
  // down.
  data()->CompleteRead();
  MessageLoop::current()->RunUntilIdle();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

// Test to ensure the SpdyStream state machine does not get confused when a
// chunk becomes available while a write is pending.
TEST_F(SpdyHttpStreamSpdy3Test, DelayedSendChunkedPost) {
  const char kUploadData1[] = "12345678";
  const int kUploadData1Size = arraysize(kUploadData1)-1;
  scoped_ptr<SpdyFrame> req(ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> chunk1(ConstructSpdyBodyFrame(1, false));
  scoped_ptr<SpdyFrame> chunk2(
      ConstructSpdyBodyFrame(1, kUploadData1, kUploadData1Size, false));
  scoped_ptr<SpdyFrame> chunk3(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0),
    CreateMockWrite(*chunk1, 1),  // POST upload frames
    CreateMockWrite(*chunk2, 2),
    CreateMockWrite(*chunk3, 3),
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  MockRead reads[] = {
    CreateMockRead(*resp, 4),
    CreateMockRead(*chunk1, 5),
    CreateMockRead(*chunk2, 6),
    CreateMockRead(*chunk3, 7),
    MockRead(ASYNC, 0, 8)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());

  ASSERT_EQ(OK, InitSessionDeterministic(reads, arraysize(reads),
                                         writes, arraysize(writes),
                                         host_port_pair));

  UploadDataStream upload_stream(UploadDataStream::CHUNKED, 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_stream;

  ASSERT_EQ(OK, upload_stream.Init(CompletionCallback()));
  upload_stream.AppendChunk(kUploadData, kUploadDataSize, false);

  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(
      new SpdyHttpStream(session_.get(), true));
  ASSERT_EQ(OK, http_stream->InitializeStream(&request, DEFAULT_PRIORITY,
                                              net_log, CompletionCallback()));

  TestCompletionCallback callback;
  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  // Complete the initial request write and the first chunk.
  deterministic_data()->RunFor(2);
  ASSERT_TRUE(callback.have_result());
  EXPECT_GT(callback.WaitForResult(), 0);

  // Now append the final two chunks which will enqueue two more writes.
  upload_stream.AppendChunk(kUploadData1, kUploadData1Size, false);
  upload_stream.AppendChunk(kUploadData, kUploadDataSize, true);

  // Finish writing all the chunks.
  deterministic_data()->RunFor(2);

  // Read response headers.
  deterministic_data()->RunFor(1);
  ASSERT_EQ(OK, http_stream->ReadResponseHeaders(callback.callback()));

  // Read and check |chunk1| response.
  deterministic_data()->RunFor(1);
  scoped_refptr<IOBuffer> buf1(new IOBuffer(kUploadDataSize));
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(buf1,
                                          kUploadDataSize,
                                          callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf1->data(), kUploadDataSize));

  // Read and check |chunk2| response.
  deterministic_data()->RunFor(1);
  scoped_refptr<IOBuffer> buf2(new IOBuffer(kUploadData1Size));
  ASSERT_EQ(kUploadData1Size,
            http_stream->ReadResponseBody(buf2,
                                          kUploadData1Size,
                                          callback.callback()));
  EXPECT_EQ(kUploadData1, std::string(buf2->data(), kUploadData1Size));

  // Read and check |chunk3| response.
  deterministic_data()->RunFor(1);
  scoped_refptr<IOBuffer> buf3(new IOBuffer(kUploadDataSize));
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(buf3,
                                          kUploadDataSize,
                                          callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf3->data(), kUploadDataSize));

  // Finish reading the |EOF|.
  deterministic_data()->RunFor(1);
  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
  EXPECT_TRUE(deterministic_data()->at_read_eof());
  EXPECT_TRUE(deterministic_data()->at_write_eof());
}

// Test the receipt of a WINDOW_UPDATE frame while waiting for a chunk to be
// made available is handled correctly.
TEST_F(SpdyHttpStreamSpdy3Test, DelayedSendChunkedPostWithWindowUpdate) {
  scoped_ptr<SpdyFrame> req(ConstructChunkedSpdyPost(NULL, 0));
  scoped_ptr<SpdyFrame> chunk1(ConstructSpdyBodyFrame(1, true));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0),
    CreateMockWrite(*chunk1, 1),
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyPostSynReply(NULL, 0));
  scoped_ptr<SpdyFrame> window_update(
      ConstructSpdyWindowUpdate(1, kUploadDataSize));
  MockRead reads[] = {
    CreateMockRead(*window_update, 2),
    CreateMockRead(*resp, 3),
    CreateMockRead(*chunk1, 4),
    MockRead(ASYNC, 0, 5)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());

  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));

  DeterministicMockClientSocketFactory* socket_factory =
      session_deps_.deterministic_socket_factory.get();
  socket_factory->AddSocketDataProvider(&data);

  http_session_ = SpdySessionDependencies::SpdyCreateSessionDeterministic(
      &session_deps_);
  session_ = http_session_->spdy_session_pool()->Get(pair, BoundNetLog());
  transport_params_ = new TransportSocketParams(host_port_pair,
                                                MEDIUM, false, false,
                                                OnHostResolutionCallback());

  TestCompletionCallback callback;
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);

  EXPECT_EQ(ERR_IO_PENDING,
            connection->Init(host_port_pair.ToString(),
                             transport_params_,
                             MEDIUM,
                             callback.callback(),
                             http_session_->GetTransportSocketPool(
                                 HttpNetworkSession::NORMAL_SOCKET_POOL),
                             BoundNetLog()));

  callback.WaitForResult();
  EXPECT_EQ(OK,
            session_->InitializeWithSocket(connection.release(), false, OK));

  UploadDataStream upload_stream(UploadDataStream::CHUNKED, 0);

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data_stream = &upload_stream;

  ASSERT_EQ(OK, upload_stream.Init(CompletionCallback()));
  upload_stream.AppendChunk(kUploadData, kUploadDataSize, true);

  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(
      new SpdyHttpStream(session_.get(), true));
  ASSERT_EQ(OK, http_stream->InitializeStream(&request, DEFAULT_PRIORITY,
                                              net_log, CompletionCallback()));

  HttpRequestHeaders headers;
  HttpResponseInfo response;
  // This will attempt to Write() the initial request and headers, which will
  // complete asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  // Complete the initial request write and first chunk.
  data.RunFor(2);
  ASSERT_TRUE(callback.have_result());
  EXPECT_GT(callback.WaitForResult(), 0);

  // Verify that the window size has decreased.
  ASSERT_TRUE(http_stream->stream() != NULL);
  EXPECT_NE(static_cast<int>(kSpdyStreamInitialWindowSize),
            http_stream->stream()->send_window_size());

  // Read window update.
  data.RunFor(1);

  // Verify the window update.
  ASSERT_TRUE(http_stream->stream() != NULL);
  EXPECT_EQ(static_cast<int>(kSpdyStreamInitialWindowSize),
            http_stream->stream()->send_window_size());

  // Read response headers.
  data.RunFor(1);
  ASSERT_EQ(OK, http_stream->ReadResponseHeaders(callback.callback()));

  // Read and check |chunk1| response.
  data.RunFor(1);
  scoped_refptr<IOBuffer> buf1(new IOBuffer(kUploadDataSize));
  ASSERT_EQ(kUploadDataSize,
            http_stream->ReadResponseBody(buf1,
                                          kUploadDataSize,
                                          callback.callback()));
  EXPECT_EQ(kUploadData, std::string(buf1->data(), kUploadDataSize));

  // Finish reading the |EOF|.
  data.RunFor(1);
  ASSERT_TRUE(response.headers.get());
  ASSERT_EQ(200, response.headers->response_code());
  EXPECT_TRUE(data.at_read_eof());
  EXPECT_TRUE(data.at_write_eof());
}

// Test case for bug: http://code.google.com/p/chromium/issues/detail?id=50058
TEST_F(SpdyHttpStreamSpdy3Test, SpdyURLTest) {
  const char * const full_url = "http://www.google.com/foo?query=what#anchor";
  const char * const base_url = "http://www.google.com/foo?query=what";
  scoped_ptr<SpdyFrame> req(ConstructSpdyGet(base_url, false, 1, LOWEST));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 1),
  };
  scoped_ptr<SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  MockRead reads[] = {
    CreateMockRead(*resp, 2),
    MockRead(SYNCHRONOUS, 0, 3)  // EOF
  };

  HostPortPair host_port_pair("www.google.com", 80);
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());
  EXPECT_EQ(OK, InitSession(reads, arraysize(reads), writes, arraysize(writes),
      host_port_pair));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL(full_url);
  TestCompletionCallback callback;
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(new SpdyHttpStream(session_, true));
  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(&request, DEFAULT_PRIORITY,
                                    net_log, CompletionCallback()));

  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));

  const SpdyHeaderBlock& spdy_header =
    http_stream->stream()->spdy_headers();
  if (spdy_header.find(":path") != spdy_header.end())
    EXPECT_EQ("/foo?query=what", spdy_header.find(":path")->second);
  else
    FAIL() << "No url is set in spdy_header!";

  // This triggers the MockWrite and read 2
  callback.WaitForResult();

  // This triggers read 3. The empty read causes the session to shut down.
  data()->CompleteRead();

  // Because we abandoned the stream, we don't expect to find a session in the
  // pool anymore.
  EXPECT_FALSE(http_session_->spdy_session_pool()->HasSession(pair));
  EXPECT_TRUE(data()->at_read_eof());
  EXPECT_TRUE(data()->at_write_eof());
}

namespace {

void GetECServerBoundCertAndProof(
    const std::string& origin,
    ServerBoundCertService* server_bound_cert_service,
    std::string* cert,
    std::string* proof) {
  TestCompletionCallback callback;
  std::vector<uint8> requested_cert_types;
  requested_cert_types.push_back(CLIENT_CERT_ECDSA_SIGN);
  SSLClientCertType cert_type;
  std::string key;
  ServerBoundCertService::RequestHandle request_handle;
  int rv = server_bound_cert_service->GetDomainBoundCert(
      origin, requested_cert_types, &cert_type, &key, cert, callback.callback(),
      &request_handle);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_EQ(CLIENT_CERT_ECDSA_SIGN, cert_type);

  SpdyCredential credential;
  SpdyCredentialBuilder::Build(MockClientSocket::kTlsUnique, cert_type, key,
                               *cert, 2, &credential);

  cert->assign(credential.certs[0]);
  proof->assign(credential.proof);
}

}  // namespace

// Constructs a standard SPDY SYN_STREAM frame for a GET request with
// a credential set.
SpdyFrame* ConstructCredentialRequestFrame(size_t slot, const GURL& url,
                                           SpdyStreamId stream_id) {
  const SpdyHeaderInfo syn_headers = {
    SYN_STREAM,
    stream_id,
    0,
    ConvertRequestPriorityToSpdyPriority(LOWEST, 3),
    slot,
    CONTROL_FLAG_FIN,
    false,
    RST_STREAM_INVALID,
    NULL,
    0,
    DATA_FLAG_NONE
  };

  // TODO(rch): this is ugly.  Clean up.
  std::string str_path = url.PathForRequest();
  std::string str_scheme = url.scheme();
  std::string str_host = url.host();
  if (url.has_port()) {
    str_host += ":";
    str_host += url.port();
  }
  scoped_array<char> req(new char[str_path.size() + 1]);
  scoped_array<char> scheme(new char[str_scheme.size() + 1]);
  scoped_array<char> host(new char[str_host.size() + 1]);
  memcpy(req.get(), str_path.c_str(), str_path.size());
  memcpy(scheme.get(), str_scheme.c_str(), str_scheme.size());
  memcpy(host.get(), str_host.c_str(), str_host.size());
  req.get()[str_path.size()] = '\0';
  scheme.get()[str_scheme.size()] = '\0';
  host.get()[str_host.size()] = '\0';

  const char* const headers[] = {
    ":method",
    "GET",
    ":path",
    req.get(),
    ":host",
    host.get(),
    ":scheme",
    scheme.get(),
    ":version",
    "HTTP/1.1"
  };
  return ConstructSpdyFrame(
      syn_headers, NULL, 0, headers, arraysize(headers)/2);
}

// TODO(rch): When openssl supports server bound certifictes, this
// guard can be removed
#if !defined(USE_OPENSSL)
// Test that if we request a resource for a new origin on a session that
// used domain bound certificates, that we send a CREDENTIAL frame for
// the new domain before we send the new request.
void SpdyHttpStreamSpdy3Test::TestSendCredentials(
    ServerBoundCertService* server_bound_cert_service,
    const std::string& cert,
    const std::string& proof) {
  const char* kUrl1 = "https://www.google.com/";
  const char* kUrl2 = "https://www.gmail.com/";

  SpdyCredential cred;
  cred.slot = 2;
  cred.proof = proof;
  cred.certs.push_back(cert);

  scoped_ptr<SpdyFrame> req(ConstructCredentialRequestFrame(
      1, GURL(kUrl1), 1));
  scoped_ptr<SpdyFrame> credential(ConstructSpdyCredential(cred));
  scoped_ptr<SpdyFrame> req2(ConstructCredentialRequestFrame(
      2, GURL(kUrl2), 3));
  MockWrite writes[] = {
    CreateMockWrite(*req.get(), 0),
    CreateMockWrite(*credential.get(), 2),
    CreateMockWrite(*req2.get(), 3),
  };

  scoped_ptr<SpdyFrame> resp(ConstructSpdyGetSynReply(NULL, 0, 1));
  scoped_ptr<SpdyFrame> resp2(ConstructSpdyGetSynReply(NULL, 0, 3));
  MockRead reads[] = {
    CreateMockRead(*resp, 1),
    CreateMockRead(*resp2, 4),
    MockRead(SYNCHRONOUS, 0, 5)  // EOF
  };

  HostPortPair host_port_pair(HostPortPair::FromURL(GURL(kUrl1)));
  HostPortProxyPair pair(host_port_pair, ProxyServer::Direct());

  DeterministicMockClientSocketFactory* socket_factory =
      session_deps_.deterministic_socket_factory.get();
  DeterministicSocketData data(reads, arraysize(reads),
                               writes, arraysize(writes));
  socket_factory->AddSocketDataProvider(&data);
  SSLSocketDataProvider ssl(SYNCHRONOUS, OK);
  ssl.channel_id_sent = true;
  ssl.server_bound_cert_service = server_bound_cert_service;
  ssl.protocol_negotiated = kProtoSPDY3;
  socket_factory->AddSSLSocketDataProvider(&ssl);
  http_session_ = SpdySessionDependencies::SpdyCreateSessionDeterministic(
      &session_deps_);
  session_ = http_session_->spdy_session_pool()->Get(pair, BoundNetLog());
  transport_params_ = new TransportSocketParams(host_port_pair,
                                                MEDIUM, false, false,
                                                OnHostResolutionCallback());
  TestCompletionCallback callback;
  scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
  SSLConfig ssl_config;
  scoped_refptr<SOCKSSocketParams> socks_params;
  scoped_refptr<HttpProxySocketParams> http_proxy_params;
  scoped_refptr<SSLSocketParams> ssl_params(
      new SSLSocketParams(transport_params_,
                          socks_params,
                          http_proxy_params,
                          ProxyServer::SCHEME_DIRECT,
                          host_port_pair,
                          ssl_config,
                          0,
                          false,
                          false));
  EXPECT_EQ(ERR_IO_PENDING,
            connection->Init(host_port_pair.ToString(),
                             ssl_params,
                             MEDIUM,
                             callback.callback(),
                             http_session_->GetSSLSocketPool(
                                 HttpNetworkSession::NORMAL_SOCKET_POOL),
                             BoundNetLog()));
  callback.WaitForResult();
  EXPECT_EQ(OK,
            session_->InitializeWithSocket(connection.release(), true, OK));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL(kUrl1);
  HttpResponseInfo response;
  HttpRequestHeaders headers;
  BoundNetLog net_log;
  scoped_ptr<SpdyHttpStream> http_stream(
      new SpdyHttpStream(session_.get(), true));
  ASSERT_EQ(
      OK,
      http_stream->InitializeStream(&request, DEFAULT_PRIORITY,
                                    net_log, CompletionCallback()));

  //  EXPECT_FALSE(session_->NeedsCredentials(request.url));
  //  GURL new_origin(kUrl2);
  //  EXPECT_TRUE(session_->NeedsCredentials(new_origin));

  EXPECT_EQ(ERR_IO_PENDING, http_stream->SendRequest(headers, &response,
                                                     callback.callback()));
  EXPECT_TRUE(http_session_->spdy_session_pool()->HasSession(pair));

  data.RunFor(2);
  callback.WaitForResult();

  // Start up second request for resource on a new origin.
  scoped_ptr<SpdyHttpStream> http_stream2(
      new SpdyHttpStream(session_.get(), true));
  request.url = GURL(kUrl2);
  ASSERT_EQ(
      OK,
      http_stream2->InitializeStream(&request, DEFAULT_PRIORITY,
                                     net_log, CompletionCallback()));
  EXPECT_EQ(ERR_IO_PENDING, http_stream2->SendRequest(headers, &response,
                                                      callback.callback()));
  data.RunFor(2);
  callback.WaitForResult();

  EXPECT_EQ(ERR_IO_PENDING, http_stream2->ReadResponseHeaders(
      callback.callback()));
  data.RunFor(1);
  EXPECT_EQ(OK, callback.WaitForResult());
  ASSERT_TRUE(response.headers.get() != NULL);
  ASSERT_EQ(200, response.headers->response_code());
}

TEST_F(SpdyHttpStreamSpdy3Test, SendCredentialsEC) {
  scoped_refptr<base::SequencedWorkerPool> sequenced_worker_pool =
      new base::SequencedWorkerPool(1, "SpdyHttpStreamSpdy3Test");
  scoped_ptr<ServerBoundCertService> server_bound_cert_service(
      new ServerBoundCertService(new DefaultServerBoundCertStore(NULL),
                                 sequenced_worker_pool));
  std::string cert;
  std::string proof;
  GetECServerBoundCertAndProof("http://www.gmail.com/",
                               server_bound_cert_service.get(),
                               &cert, &proof);

  TestSendCredentials(server_bound_cert_service.get(), cert, proof);

  sequenced_worker_pool->Shutdown();
}

#endif  // !defined(USE_OPENSSL)

// TODO(willchan): Write a longer test for SpdyStream that exercises all
// methods.

}  // namespace net
