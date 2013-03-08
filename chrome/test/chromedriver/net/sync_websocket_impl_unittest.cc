// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time.h"
#include "chrome/test/chromedriver/net/sync_websocket_impl.h"
#include "chrome/test/chromedriver/net/test_http_server.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SyncWebSocketImplTest : public testing::Test {
 public:
  SyncWebSocketImplTest()
      : client_thread_("ClientThread") {}
  virtual ~SyncWebSocketImplTest() {}

  virtual void SetUp() OVERRIDE {
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    ASSERT_TRUE(client_thread_.StartWithOptions(options));
    context_getter_ = new URLRequestContextGetter(
        client_thread_.message_loop_proxy());
    ASSERT_TRUE(server_.Start());
  }

  virtual void TearDown() OVERRIDE {
    server_.Stop();
  }

 protected:
  base::Thread client_thread_;
  TestHttpServer server_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
};

}  // namespace

TEST_F(SyncWebSocketImplTest, CreateDestroy) {
  SyncWebSocketImpl sock(context_getter_);
}

TEST_F(SyncWebSocketImplTest, Connect) {
  SyncWebSocketImpl sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
}

TEST_F(SyncWebSocketImplTest, ConnectFail) {
  SyncWebSocketImpl sock(context_getter_);
  ASSERT_FALSE(sock.Connect(GURL("ws://127.0.0.1:33333")));
}

TEST_F(SyncWebSocketImplTest, SendReceive) {
  SyncWebSocketImpl sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("hi"));
  std::string message;
  ASSERT_TRUE(sock.ReceiveNextMessage(&message));
  ASSERT_STREQ("hi", message.c_str());
}

TEST_F(SyncWebSocketImplTest, SendReceiveLarge) {
  SyncWebSocketImpl sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  std::string wrote_message(10 << 20, 'a');
  ASSERT_TRUE(sock.Send(wrote_message));
  std::string message;
  ASSERT_TRUE(sock.ReceiveNextMessage(&message));
  ASSERT_EQ(wrote_message.length(), message.length());
  ASSERT_EQ(wrote_message, message);
}

TEST_F(SyncWebSocketImplTest, SendReceiveMany) {
  SyncWebSocketImpl sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
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

TEST_F(SyncWebSocketImplTest, CloseOnReceive) {
  server_.SetMessageAction(TestHttpServer::kCloseOnMessage);
  SyncWebSocketImpl sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("1"));
  std::string message;
  ASSERT_FALSE(sock.ReceiveNextMessage(&message));
  ASSERT_STREQ("", message.c_str());
}

TEST_F(SyncWebSocketImplTest, CloseOnSend) {
  SyncWebSocketImpl sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  server_.Stop();
  ASSERT_FALSE(sock.Send("1"));
}

TEST_F(SyncWebSocketImplTest, Reconnect) {
  SyncWebSocketImpl sock(context_getter_);
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_TRUE(sock.Send("1"));
  // Wait for SyncWebSocket to receive the response from the server.
  base::Time deadline = base::Time::Now() + base::TimeDelta::FromSeconds(20);
  while (base::Time::Now() < deadline) {
    if (sock.IsConnected() && !sock.HasNextMessage())
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
    else
      break;
  }
  server_.Stop();
  ASSERT_FALSE(sock.Send("2"));
  ASSERT_FALSE(sock.IsConnected());
  server_.Start();
  ASSERT_TRUE(sock.HasNextMessage());
  ASSERT_TRUE(sock.Connect(server_.web_socket_url()));
  ASSERT_FALSE(sock.HasNextMessage());
  ASSERT_TRUE(sock.Send("3"));
  std::string message;
  ASSERT_TRUE(sock.ReceiveNextMessage(&message));
  ASSERT_STREQ("3", message.c_str());
  ASSERT_FALSE(sock.HasNextMessage());
}
