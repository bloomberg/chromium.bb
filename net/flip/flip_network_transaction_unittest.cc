// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/flip/flip_network_transaction.h"

#include "base/ref_counted.h"
#include "net/base/completion_callback.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/ssl_config_service_defaults.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/flip/flip_protocol.h"
#include "net/http/http_network_session.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/socket/socket_test_util.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

class FlipStreamParserPeer {
 public:
  explicit FlipStreamParserPeer(FlipStreamParser* flip_stream_parser)
      : flip_stream_parser_(flip_stream_parser) {}

  int flip_stream_id() const { return flip_stream_parser_->flip_stream_id_; }

 private:
  FlipStreamParser* const flip_stream_parser_;

  DISALLOW_COPY_AND_ASSIGN(FlipStreamParserPeer);
};

namespace {

// Create a proxy service which fails on all requests (falls back to direct).
ProxyService* CreateNullProxyService() {
  return ProxyService::CreateNull();
}

// Helper to manage the lifetimes of the dependencies for a
// FlipNetworkTransaction.
class SessionDependencies {
 public:
  // Default set of dependencies -- "null" proxy service.
  SessionDependencies()
      : host_resolver(new MockHostResolver),
        proxy_service(CreateNullProxyService()),
        ssl_config_service(new SSLConfigServiceDefaults),
        flip_session_pool(new FlipSessionPool) {}

  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : host_resolver(new MockHostResolver),
        proxy_service(proxy_service),
        ssl_config_service(new SSLConfigServiceDefaults),
        flip_session_pool(new FlipSessionPool) {}

  scoped_refptr<MockHostResolverBase> host_resolver;
  scoped_refptr<ProxyService> proxy_service;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockClientSocketFactory socket_factory;
  scoped_refptr<FlipSessionPool> flip_session_pool;
};

ProxyService* CreateFixedProxyService(const std::string& proxy) {
  ProxyConfig proxy_config;
  proxy_config.proxy_rules.ParseFromString(proxy);
  return ProxyService::CreateFixed(proxy_config);
}


HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(session_deps->host_resolver,
                                session_deps->proxy_service,
                                &session_deps->socket_factory,
                                session_deps->ssl_config_service,
                                session_deps->flip_session_pool);
}

class FlipStreamParserTest : public PlatformTest {
 protected:
  FlipStreamParserTest()
      : session_(CreateSession(&session_deps_)),
        parser_peer_(&parser_) {}

  FlipSession* CreateFlipSession() {
    HostResolver::RequestInfo resolve_info("www.google.com", 80);
    FlipSession* session = session_->flip_session_pool()->Get(
        resolve_info, session_);
    return session;
  }

  virtual void TearDown() {
    MessageLoop::current()->RunAllPending();
    PlatformTest::TearDown();
  }

  SessionDependencies session_deps_;
  scoped_refptr<HttpNetworkSession> session_;
  FlipStreamParser parser_;
  FlipStreamParserPeer parser_peer_;
};

// TODO(willchan): Look into why TCPConnectJobs are still alive when this test
// goes away.  They're calling into the ClientSocketFactory which doesn't exist
// anymore, so it crashes.
TEST_F(FlipStreamParserTest, DISABLED_SendRequest) {
  scoped_refptr<FlipSession> flip(CreateFlipSession());
  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  TestCompletionCallback callback;

  EXPECT_EQ(ERR_IO_PENDING, parser_.SendRequest(flip, &request, &callback));
  EXPECT_TRUE(flip->IsStreamActive(parser_peer_.flip_stream_id()));
}

// TODO(willchan): Write a longer test for FlipStreamParser that exercises all
// methods.

}  // namespace

// A DataProvider where the client must write a request before the reads (e.g.
// the response) will complete.
class DelayedSocketData : public StaticSocketDataProvider,
                          public base::RefCounted<DelayedSocketData> {
 public:
  // Note: All MockReads and MockWrites must be async.
  // Note: The MockRead and MockWrite lists musts end with a EOF
  //       e.g. a MockRead(true, 0, 0);
  DelayedSocketData(MockRead* r, MockWrite* w)
    : StaticSocketDataProvider(r, w),
      request_received_(false) {}

  virtual MockRead GetNextRead() {
    if (!request_received_)
      return MockRead(true, ERR_IO_PENDING);
    return StaticSocketDataProvider::GetNextRead();
  }

  virtual MockWriteResult OnWrite(const std::string& data) {
    MockWriteResult rv = StaticSocketDataProvider::OnWrite(data);
    // Now that our write has completed, we can allow reads to continue.
    if (!request_received_) {
      request_received_ = true;
      MessageLoop::current()->PostDelayedTask(FROM_HERE,
        NewRunnableMethod(this, &DelayedSocketData::CompleteRead), 100);
    }
    return rv;
  }

  void CompleteRead() {
    socket()->OnReadComplete(GetNextRead());
  }

 private:
  bool request_received_;
};

class FlipNetworkTransactionTest : public PlatformTest {
 protected:
  virtual void SetUp() {
    // Disable compression on this test.
    flip::FlipFramer::set_enable_compression_default(false);
  }

  virtual void TearDown() {
    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
    PlatformTest::TearDown();
  }

  void KeepAliveConnectionResendRequestTest(const MockRead& read_failure);

  struct TransactionHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
  };

  TransactionHelperResult TransactionHelper(const HttpRequestInfo& request,
                                            MockRead reads[],
                                            MockWrite writes[]) {
    TransactionHelperResult out;

    // We disable SSL for this test.
    FlipSession::SetSSLMode(false);

    SessionDependencies session_deps;
    scoped_ptr<FlipNetworkTransaction> trans(
        new FlipNetworkTransaction(CreateSession(&session_deps)));

    scoped_refptr<DelayedSocketData> data(new DelayedSocketData(reads, writes));
    session_deps.socket_factory.AddSocketDataProvider(data.get());

    TestCompletionCallback callback;

    int rv = trans->Start(&request, &callback, NULL);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    out.rv = callback.WaitForResult();
    if (out.rv != OK)
      return out;

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response->headers != NULL);
    out.status_line = response->headers->GetStatusLine();

    rv = ReadTransaction(trans.get(), &out.response_data);
    EXPECT_EQ(OK, rv);

    // Verify that we consumed all test data.
    EXPECT_TRUE(data->at_read_eof());
    EXPECT_TRUE(data->at_write_eof());

    return out;
  }

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);
};

//-----------------------------------------------------------------------------

// Verify FlipNetworkTransaction constructor.
TEST_F(FlipNetworkTransactionTest, Constructor) {
  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session =
      CreateSession(&session_deps);
  scoped_ptr<HttpTransaction> trans(new FlipNetworkTransaction(session));
}

TEST_F(FlipNetworkTransactionTest, Get) {
  static const unsigned char syn[] = {
    0x80, 0x01, 0x00, 0x01,                                        // header
    0x01, 0x00, 0x00, 0x45,                                        // FIN, len
    0x00, 0x00, 0x00, 0x01,                                        // stream id
    0xc0, 0x00, 0x00, 0x03,                                        // 4 headers
    0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
    0x00, 0x03, 'G', 'E', 'T',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
                '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
                'm', '/',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  static const unsigned char syn_reply[] = {
    0x80, 0x01, 0x00, 0x02,                                        // header
    0x00, 0x00, 0x00, 0x45,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,                                        // 4 headers
    0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
    0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
    0x00, 0x03, '2', '0', '0',                                     // "200"
    0x00, 0x03, 'u', 'r', 'l',                                     // "url"
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',  // "HTTP/1.1"
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
  };
  static const unsigned char body_frame[] = {
    0x00, 0x00, 0x00, 0x01,                                        // header
    0x00, 0x00, 0x00, 0x06,
    'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
  };
  static const unsigned char fin_frame[] = {
    0x80, 0x01, 0x00, 0x03,                                        // header
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
  };

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(syn), sizeof(syn)),
    MockWrite(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(syn_reply), sizeof(syn_reply)),
    MockRead(true, reinterpret_cast<const char*>(body_frame),
             sizeof(body_frame)),
    MockRead(true, reinterpret_cast<const char*>(fin_frame),
             sizeof(fin_frame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  TransactionHelperResult out = TransactionHelper(request, reads, writes);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that a simple POST works.
TEST_F(FlipNetworkTransactionTest, Post) {
  static const char upload[] = { "hello world" };

  // Setup the request
  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");
  request.upload_data = new UploadData();
  request.upload_data->AppendBytes(upload, sizeof(upload));

  // TODO(mbelshe): Hook up the write validation.

  static const unsigned char syn[] = {
    0x80, 0x01, 0x00, 0x01,                                      // header
    0x00, 0x00, 0x00, 0x46,                                      // flags, len
    0x00, 0x00, 0x00, 0x01,                                      // stream id
    0xc0, 0x00, 0x00, 0x03,                                      // 4 headers
    0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
    0x00, 0x04, 'P', 'O', 'S', 'T',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
                '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
                'm', '/',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  static const unsigned char upload_frame[] = {
    0x00, 0x00, 0x00, 0x01,                                        // header
    0x01, 0x00, 0x00, 0x0c,                                        // FIN flag
    'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd', '\0'
  };

  // The response
  static const unsigned char syn_reply[] = {
    0x80, 0x01, 0x00, 0x02,                                        // header
    0x00, 0x00, 0x00, 0x45,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,                                        // 4 headers
    0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
    0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
    0x00, 0x03, '2', '0', '0',                                     // "200"
    0x00, 0x03, 'u', 'r', 'l',                                     // "url"
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',  // "HTTP/1.1"
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
  };
  static const unsigned char body_frame[] = {
    0x00, 0x00, 0x00, 0x01,                                        // header
    0x00, 0x00, 0x00, 0x06,
    'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
  };
  static const unsigned char fin_frame[] = {
    0x80, 0x01, 0x00, 0x03,                                        // header
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
  };

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(syn), sizeof(syn)),
    MockWrite(true, reinterpret_cast<const char*>(upload_frame),
              sizeof(upload_frame)),
    MockWrite(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(syn_reply),
             sizeof(syn_reply)),
    MockRead(true, reinterpret_cast<const char*>(body_frame),
             sizeof(body_frame)),
    MockRead(true, reinterpret_cast<const char*>(fin_frame),
             sizeof(fin_frame)),
    MockRead(true, 0, 0)  // EOF
  };

  TransactionHelperResult out = TransactionHelper(request, reads, writes);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 200 OK", out.status_line);
  EXPECT_EQ("hello!", out.response_data);
}

// Test that the transaction doesn't crash when we don't have a reply.
TEST_F(FlipNetworkTransactionTest, ResponseWithoutSynReply) {
  static const unsigned char body_frame[] = {
    0x00, 0x00, 0x00, 0x01,                                        // header
    0x00, 0x00, 0x00, 0x06,
    'h', 'e', 'l', 'l', 'o', '!',                                  // "hello"
  };
  static const unsigned char fin_frame[] = {
    0x80, 0x01, 0x00, 0x03,                                        // header
    0x00, 0x00, 0x00, 0x08,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00,
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(body_frame),
             sizeof(body_frame)),
    MockRead(true, reinterpret_cast<const char*>(fin_frame), sizeof(fin_frame)),
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  TransactionHelperResult out = TransactionHelper(request, reads, NULL);
  EXPECT_EQ(ERR_SYN_REPLY_NOT_RECEIVED, out.rv);
}

// TODO(willchan): Look into why TCPConnectJobs are still alive when this test
// goes away.  They're calling into the ClientSocketFactory which doesn't exist
// anymore, so it crashes.
TEST_F(FlipNetworkTransactionTest, DISABLED_CancelledTransaction) {
  static const unsigned char syn[] = {
    0x80, 0x01, 0x00, 0x01,                                        // header
    0x01, 0x00, 0x00, 0x45,                                        // FIN, len
    0x00, 0x00, 0x00, 0x01,                                        // stream id
    0xc0, 0x00, 0x00, 0x03,                                        // 4 headers
    0x00, 0x06, 'm', 'e', 't', 'h', 'o', 'd',
    0x00, 0x03, 'G', 'E', 'T',
    0x00, 0x03, 'u', 'r', 'l',
    0x00, 0x16, 'h', 't', 't', 'p', ':', '/', '/', 'w', 'w', 'w',
                '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o',
                'm', '/',
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',
  };

  static const unsigned char syn_reply[] = {
    0x80, 0x01, 0x00, 0x02,                                        // header
    0x00, 0x00, 0x00, 0x45,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x04,                                        // 4 headers
    0x00, 0x05, 'h', 'e', 'l', 'l', 'o',                           // "hello"
    0x00, 0x03, 'b', 'y', 'e',                                     // "bye"
    0x00, 0x06, 's', 't', 'a', 't', 'u', 's',                      // "status"
    0x00, 0x03, '2', '0', '0',                                     // "200"
    0x00, 0x03, 'u', 'r', 'l',                                     // "url"
    0x00, 0x0a, '/', 'i', 'n', 'd', 'e', 'x', '.', 'p', 'h', 'p',  // "HTTP/1.1"
    0x00, 0x07, 'v', 'e', 'r', 's', 'i', 'o', 'n',                 // "version"
    0x00, 0x08, 'H', 'T', 'T', 'P', '/', '1', '.', '1',            // "HTTP/1.1"
  };

  MockWrite writes[] = {
    MockWrite(true, reinterpret_cast<const char*>(syn), sizeof(syn)),
    MockRead(true, 0, 0)  // EOF
  };

  MockRead reads[] = {
    MockRead(true, reinterpret_cast<const char*>(syn_reply), sizeof(syn_reply)),
    // This following read isn't used by the test, except during the
    // RunAllPending() call at the end since the FlipSession survives the
    // FlipNetworkTransaction and still tries to continue Read()'ing.  Any
    // MockRead will do here.
    MockRead(true, 0, 0)  // EOF
  };

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // We disable SSL for this test.
  FlipSession::SetSSLMode(false);

  SessionDependencies session_deps;
  scoped_ptr<FlipNetworkTransaction> trans(
      new FlipNetworkTransaction(CreateSession(&session_deps)));

  StaticSocketDataProvider data(reads, writes);
  session_deps.socket_factory.AddSocketDataProvider(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  trans.reset();  // Cancel the transaction.

  // Flush the MessageLoop while the SessionDependencies (in particular, the
  // MockClientSocketFactory) are still alive.
  MessageLoop::current()->RunAllPending();
}

}  // namespace net
