// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_listen_socket.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

void SetTimedOutAndQuitLoop(const base::WeakPtr<bool> timed_out,
                            const base::Closure& quit_loop_func) {
  if (timed_out) {
    *timed_out = true;
    quit_loop_func.Run();
  }
}

bool RunLoopWithTimeout(base::RunLoop* run_loop) {
  bool timed_out = false;
  base::WeakPtrFactory<bool> timed_out_weak_factory(&timed_out);
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SetTimedOutAndQuitLoop,
                 timed_out_weak_factory.GetWeakPtr(),
                 run_loop->QuitClosure()),
      base::TimeDelta::FromSeconds(1));
  run_loop->Run();
  return !timed_out;
}

class TestHttpClient {
 public:
  TestHttpClient() : connect_result_(OK) {}

  int ConnectAndWait(const IPEndPoint& address) {
    AddressList addresses(address);
    NetLog::Source source;
    socket_.reset(new TCPClientSocket(addresses, NULL, source));

    base::RunLoop run_loop;
    connect_result_ = socket_->Connect(base::Bind(&TestHttpClient::OnConnect,
                                                  base::Unretained(this),
                                                  run_loop.QuitClosure()));
    if (connect_result_ != OK && connect_result_ != ERR_IO_PENDING)
      return connect_result_;

    if (!RunLoopWithTimeout(&run_loop))
      return ERR_TIMED_OUT;
    return connect_result_;
  }

  void Send(const std::string& data) {
    write_buffer_ =
        new DrainableIOBuffer(new StringIOBuffer(data), data.length());
    Write();
  }

 private:
  void OnConnect(const base::Closure& quit_loop, int result) {
    connect_result_ = result;
    quit_loop.Run();
  }

  void Write() {
    int result = socket_->Write(
        write_buffer_.get(),
        write_buffer_->BytesRemaining(),
        base::Bind(&TestHttpClient::OnWrite, base::Unretained(this)));
    if (result != ERR_IO_PENDING)
      OnWrite(result);
  }

  void OnWrite(int result) {
    ASSERT_GT(result, 0);
    write_buffer_->DidConsume(result);
    if (write_buffer_->BytesRemaining())
      Write();
  }

  scoped_refptr<DrainableIOBuffer> write_buffer_;
  scoped_ptr<TCPClientSocket> socket_;
  int connect_result_;
};

}  // namespace

class HttpServerTest : public testing::Test,
                       public HttpServer::Delegate {
 public:
  HttpServerTest() : quit_after_request_count_(0) {}

  virtual void SetUp() OVERRIDE {
    TCPListenSocketFactory socket_factory("127.0.0.1", 0);
    server_ = new HttpServer(socket_factory, this);
    ASSERT_EQ(OK, server_->GetLocalAddress(&server_address_));
  }

  virtual void OnHttpRequest(int connection_id,
                             const HttpServerRequestInfo& info) OVERRIDE {
    requests_.push_back(info);
    if (requests_.size() == quit_after_request_count_)
      run_loop_quit_func_.Run();
  }

  virtual void OnWebSocketRequest(int connection_id,
                                  const HttpServerRequestInfo& info) OVERRIDE {
    NOTREACHED();
  }

  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE {
    NOTREACHED();
  }

  virtual void OnClose(int connection_id) OVERRIDE {}

  bool RunUntilRequestsReceived(size_t count) {
    quit_after_request_count_ = count;
    if (requests_.size() == count)
      return true;

    base::RunLoop run_loop;
    run_loop_quit_func_ = run_loop.QuitClosure();
    bool success = RunLoopWithTimeout(&run_loop);
    run_loop_quit_func_.Reset();
    return success;
  }

 protected:
  scoped_refptr<HttpServer> server_;
  IPEndPoint server_address_;
  base::Closure run_loop_quit_func_;
  std::vector<HttpServerRequestInfo> requests_;

 private:
  size_t quit_after_request_count_;
};

TEST_F(HttpServerTest, Request) {
  TestHttpClient client;
  ASSERT_EQ(OK, client.ConnectAndWait(server_address_));
  client.Send("GET /test HTTP/1.1\r\n\r\n");
  ASSERT_TRUE(RunUntilRequestsReceived(1));
  ASSERT_EQ("GET", requests_[0].method);
  ASSERT_EQ("/test", requests_[0].path);
  ASSERT_EQ("", requests_[0].data);
  ASSERT_EQ(0u, requests_[0].headers.size());
  ASSERT_TRUE(StartsWithASCII(requests_[0].peer.ToString(), "127.0.0.1", true));
}

TEST_F(HttpServerTest, RequestWithHeaders) {
  TestHttpClient client;
  ASSERT_EQ(OK, client.ConnectAndWait(server_address_));
  const char* kHeaders[][3] = {
      {"Header", ": ", "1"},
      {"HeaderWithNoWhitespace", ":", "1"},
      {"HeaderWithWhitespace", "   :  \t   ", "1 1 1 \t  "},
      {"HeaderWithColon", ": ", "1:1"},
      {"EmptyHeader", ":", ""},
      {"EmptyHeaderWithWhitespace", ":  \t  ", ""},
      {"HeaderWithNonASCII", ":  ", "\xf7"},
  };
  std::string headers;
  for (size_t i = 0; i < arraysize(kHeaders); ++i) {
    headers +=
        std::string(kHeaders[i][0]) + kHeaders[i][1] + kHeaders[i][2] + "\r\n";
  }

  client.Send("GET /test HTTP/1.1\r\n" + headers + "\r\n");
  ASSERT_TRUE(RunUntilRequestsReceived(1));
  ASSERT_EQ("", requests_[0].data);

  for (size_t i = 0; i < arraysize(kHeaders); ++i) {
    std::string field = StringToLowerASCII(std::string(kHeaders[i][0]));
    std::string value = kHeaders[i][2];
    ASSERT_EQ(1u, requests_[0].headers.count(field)) << field;
    ASSERT_EQ(value, requests_[0].headers[field]) << kHeaders[i][0];
  }
}

TEST_F(HttpServerTest, RequestWithBody) {
  TestHttpClient client;
  ASSERT_EQ(OK, client.ConnectAndWait(server_address_));
  std::string body = "a" + std::string(1 << 10, 'b') + "c";
  client.Send(base::StringPrintf(
      "GET /test HTTP/1.1\r\n"
      "SomeHeader: 1\r\n"
      "Content-Length: %" PRIuS "\r\n\r\n%s",
      body.length(),
      body.c_str()));
  ASSERT_TRUE(RunUntilRequestsReceived(1));
  ASSERT_EQ(2u, requests_[0].headers.size());
  ASSERT_EQ(body.length(), requests_[0].data.length());
  ASSERT_EQ('a', body[0]);
  ASSERT_EQ('c', *body.rbegin());
}

TEST_F(HttpServerTest, RequestWithTooLargeBody) {
  class TestURLFetcherDelegate : public URLFetcherDelegate {
   public:
    TestURLFetcherDelegate(const base::Closure& quit_loop_func)
        : quit_loop_func_(quit_loop_func) {}
    virtual ~TestURLFetcherDelegate() {}

    virtual void OnURLFetchComplete(const URLFetcher* source) OVERRIDE {
      EXPECT_EQ(HTTP_INTERNAL_SERVER_ERROR, source->GetResponseCode());
      quit_loop_func_.Run();
    }

   private:
    base::Closure quit_loop_func_;
  };

  base::RunLoop run_loop;
  TestURLFetcherDelegate delegate(run_loop.QuitClosure());

  scoped_refptr<URLRequestContextGetter> request_context_getter(
      new TestURLRequestContextGetter(base::MessageLoopProxy::current()));
  scoped_ptr<URLFetcher> fetcher(
      URLFetcher::Create(GURL(base::StringPrintf("http://127.0.0.1:%d/test",
                                                 server_address_.port())),
                         URLFetcher::GET,
                         &delegate));
  fetcher->SetRequestContext(request_context_getter.get());
  fetcher->AddExtraRequestHeader(
      base::StringPrintf("content-length:%d", 1 << 30));
  fetcher->Start();

  ASSERT_TRUE(RunLoopWithTimeout(&run_loop));
  ASSERT_EQ(0u, requests_.size());
}

namespace {

class MockStreamListenSocket : public StreamListenSocket {
 public:
  MockStreamListenSocket(StreamListenSocket::Delegate* delegate)
      : StreamListenSocket(kInvalidSocket, delegate) {}

  virtual void Accept() OVERRIDE { NOTREACHED(); }

 private:
  virtual ~MockStreamListenSocket() {}
};

}  // namespace

TEST_F(HttpServerTest, RequestWithBodySplitAcrossPackets) {
  StreamListenSocket* socket =
      new MockStreamListenSocket(server_.get());
  server_->DidAccept(NULL, make_scoped_ptr(socket));
  std::string body("body");
  std::string request = base::StringPrintf(
      "GET /test HTTP/1.1\r\n"
      "SomeHeader: 1\r\n"
      "Content-Length: %" PRIuS "\r\n\r\n%s",
      body.length(),
      body.c_str());
  server_->DidRead(socket, request.c_str(), request.length() - 2);
  ASSERT_EQ(0u, requests_.size());
  server_->DidRead(socket, request.c_str() + request.length() - 2, 2);
  ASSERT_EQ(1u, requests_.size());
  ASSERT_EQ(body, requests_[0].data);
}

TEST_F(HttpServerTest, MultipleRequestsOnSameConnection) {
  // The idea behind this test is that requests with or without bodies should
  // not break parsing of the next request.
  TestHttpClient client;
  ASSERT_EQ(OK, client.ConnectAndWait(server_address_));
  std::string body = "body";
  client.Send(base::StringPrintf(
      "GET /test HTTP/1.1\r\n"
      "Content-Length: %" PRIuS "\r\n\r\n%s",
      body.length(),
      body.c_str()));
  ASSERT_TRUE(RunUntilRequestsReceived(1));
  ASSERT_EQ(body, requests_[0].data);

  client.Send("GET /test2 HTTP/1.1\r\n\r\n");
  ASSERT_TRUE(RunUntilRequestsReceived(2));
  ASSERT_EQ("/test2", requests_[1].path);

  client.Send("GET /test3 HTTP/1.1\r\n\r\n");
  ASSERT_TRUE(RunUntilRequestsReceived(3));
  ASSERT_EQ("/test3", requests_[2].path);
}

}  // namespace net
