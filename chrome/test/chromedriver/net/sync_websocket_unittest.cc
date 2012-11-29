// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "base/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_listen_socket.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SyncWebSocketTest : public testing::Test,
                          public net::HttpServer::Delegate {
 public:
  SyncWebSocketTest()
      : io_thread_("io"),
        close_on_receive_(false) {
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(options));
    context_getter_ = new URLRequestContextGetter(
        io_thread_.message_loop_proxy());
    base::WaitableEvent event(false, false);
    io_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&SyncWebSocketTest::InitOnIO,
                   base::Unretained(this), &event));
    event.Wait();
  }

  virtual ~SyncWebSocketTest() {
    base::WaitableEvent event(false, false);
    io_thread_.message_loop_proxy()->PostTask(
        FROM_HERE,
        base::Bind(&SyncWebSocketTest::DestroyServerOnIO,
                   base::Unretained(this), &event));
    event.Wait();
  }

  void InitOnIO(base::WaitableEvent* event) {
    net::TCPListenSocketFactory factory("127.0.0.1", 0);
    server_ = new net::HttpServer(factory, this);
    net::IPEndPoint address;
    CHECK_EQ(net::OK, server_->GetLocalAddress(&address));
    server_url_ = GURL(base::StringPrintf("ws://127.0.0.1:%d", address.port()));
    event->Signal();
  }

  void DestroyServerOnIO(base::WaitableEvent* event) {
    server_ = NULL;
    event->Signal();
  }

  // Overridden from net::HttpServer::Delegate:
  virtual void OnHttpRequest(int connection_id,
                             const net::HttpServerRequestInfo& info) {}

  virtual void OnWebSocketRequest(int connection_id,
                                  const net::HttpServerRequestInfo& info) {
    server_->AcceptWebSocket(connection_id, info);
  }

  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) {
    if (close_on_receive_) {
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&net::HttpServer::Close, server_, connection_id));
    } else {
      server_->SendOverWebSocket(connection_id, data);
    }
  }

  virtual void OnClose(int connection_id) {}

 protected:
  base::Thread io_thread_;
  scoped_refptr<net::HttpServer> server_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  GURL server_url_;
  bool close_on_receive_;
};

}  // namespace

TEST_F(SyncWebSocketTest, CreateDestroy) {
  SyncWebSocket sock(context_getter_);
}

TEST_F(SyncWebSocketTest, Connect) {
  SyncWebSocket sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_url_));
}

TEST_F(SyncWebSocketTest, ConnectFail) {
  SyncWebSocket sock(context_getter_);
  ASSERT_FALSE(sock.Connect(GURL("ws://127.0.0.1:33333")));
}

TEST_F(SyncWebSocketTest, SendReceive) {
  SyncWebSocket sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_url_));
  ASSERT_TRUE(sock.Send("hi"));
  std::string message;
  ASSERT_TRUE(sock.ReceiveNextMessage(&message));
  ASSERT_STREQ("hi", message.c_str());
}

TEST_F(SyncWebSocketTest, SendReceiveLarge) {
  SyncWebSocket sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_url_));
  // Sends/receives 200kb. For some reason pushing this above 240kb on my
  // machine results in receiving no data back from the http server.
  std::string wrote_message(200 << 10, 'a');
  ASSERT_TRUE(sock.Send(wrote_message));
  std::string message;
  ASSERT_TRUE(sock.ReceiveNextMessage(&message));
  ASSERT_EQ(wrote_message.length(), message.length());
  ASSERT_EQ(wrote_message, message);
}

TEST_F(SyncWebSocketTest, SendReceiveMany) {
  SyncWebSocket sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_url_));
  ASSERT_TRUE(sock.Send("1"));
  ASSERT_TRUE(sock.Send("2"));
  std::string message;
  ASSERT_TRUE(sock.ReceiveNextMessage(&message));
  ASSERT_STREQ("1", message.c_str());
  ASSERT_TRUE(sock.Send("3"));
  ASSERT_TRUE(sock.ReceiveNextMessage(&message));
  ASSERT_STREQ("2", message.c_str());
  ASSERT_TRUE(sock.ReceiveNextMessage(&message));
  ASSERT_STREQ("3", message.c_str());
}

TEST_F(SyncWebSocketTest, CloseOnReceive) {
  close_on_receive_ = true;
  SyncWebSocket sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_url_));
  ASSERT_TRUE(sock.Send("1"));
  std::string message;
  ASSERT_FALSE(sock.ReceiveNextMessage(&message));
  ASSERT_STREQ("", message.c_str());
}

TEST_F(SyncWebSocketTest, CloseOnSend) {
  SyncWebSocket sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_url_));
  base::WaitableEvent event(false, false);
  io_thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&SyncWebSocketTest::DestroyServerOnIO,
                 base::Unretained(this), &event));
  event.Wait();
  ASSERT_FALSE(sock.Send("1"));
}
