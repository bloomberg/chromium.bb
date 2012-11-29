// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/test/chromedriver/net/websocket.h"
#include "googleurl/src/gurl.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/tcp_listen_socket.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void OnConnectFinished(int* save_error, int error) {
  MessageLoop::current()->Quit();
  *save_error = error;
}

class Listener : public WebSocketListener {
 public:
  explicit Listener(const std::vector<std::string>& messages)
      : messages_(messages) {}

  virtual ~Listener() {
    EXPECT_TRUE(messages_.empty());
  }

  virtual void OnMessageReceived(const std::string& message) OVERRIDE {
    ASSERT_TRUE(messages_.size());
    EXPECT_EQ(messages_[0], message);
    messages_.erase(messages_.begin());
    if (messages_.empty())
      MessageLoop::current()->Quit();
  }

  virtual void OnClose() OVERRIDE {
    EXPECT_TRUE(false);
  }

 private:
  std::vector<std::string> messages_;
};

class CloseListener : public WebSocketListener {
 public:
  explicit CloseListener(bool expect_close) : expect_close_(expect_close) {}

  virtual ~CloseListener() {
    EXPECT_FALSE(expect_close_);
  }

  virtual void OnMessageReceived(const std::string& message) OVERRIDE {}

  virtual void OnClose() OVERRIDE {
    EXPECT_TRUE(expect_close_);
    if (expect_close_)
      MessageLoop::current()->Quit();
    expect_close_ = false;
  }

 private:
  bool expect_close_;
};

class WebSocketTest : public testing::Test,
                      public net::HttpServer::Delegate {
 public:
  enum WebSocketRequestResponse {
    kAccept = 0,
    kNotFound,
    kClose
  };

  WebSocketTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(server_(CreateServer())),
        context_getter_(
            new net::TestURLRequestContextGetter(loop_.message_loop_proxy())),
        ws_request_response_(kAccept),
        close_on_message_(false),
        quit_on_close_(false) {
    net::IPEndPoint address;
    CHECK_EQ(net::OK, server_->GetLocalAddress(&address));
    server_url_ = GURL(base::StringPrintf("ws://127.0.0.1:%d", address.port()));
  }

  // Overridden from net::HttpServer::Delegate:
  virtual void OnHttpRequest(int connection_id,
                             const net::HttpServerRequestInfo& info) {}

  virtual void OnWebSocketRequest(int connection_id,
                                  const net::HttpServerRequestInfo& info) {
    switch (ws_request_response_) {
      case kAccept:
        server_->AcceptWebSocket(connection_id, info);
        break;
      case kNotFound:
        server_->Send404(connection_id);
        break;
      case kClose:
        // net::HttpServer doesn't allow us to close connection during callback.
        MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&net::HttpServer::Close, server_, connection_id));
        break;
    }
  }

  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) {
    if (close_on_message_) {
      // net::HttpServer doesn't allow us to close connection during callback.
      MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&net::HttpServer::Close, server_, connection_id));
    } else {
      server_->SendOverWebSocket(connection_id, data);
    }
  }

  virtual void OnClose(int connection_id) {
    if (quit_on_close_)
      MessageLoop::current()->Quit();
  }

 protected:
  net::HttpServer* CreateServer() {
    net::TCPListenSocketFactory factory("127.0.0.1", 0);
    return new net::HttpServer(factory, this);
  }

  scoped_ptr<WebSocket> CreateWebSocket(const GURL& url,
                                        WebSocketListener* listener) {
    int error;
    scoped_ptr<WebSocket> sock(new WebSocket(
        context_getter_, url, listener));
    sock->Connect(base::Bind(&OnConnectFinished, &error));
    loop_.PostDelayedTask(
        FROM_HERE, MessageLoop::QuitWhenIdleClosure(),
        base::TimeDelta::FromSeconds(10));
    base::RunLoop().Run();
    if (error == net::OK)
      return sock.Pass();
    return scoped_ptr<WebSocket>();
  }

  scoped_ptr<WebSocket> CreateConnectedWebSocket(WebSocketListener* listener) {
    return CreateWebSocket(server_url_, listener);
  }

  void SendReceive(const std::vector<std::string>& messages) {
    Listener listener(messages);
    scoped_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
    ASSERT_TRUE(sock);
    for (size_t i = 0; i < messages.size(); ++i) {
      ASSERT_TRUE(sock->Send(messages[i]));
    }
    base::RunLoop run_loop;
    loop_.PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromSeconds(10));
    run_loop.Run();
  }

  MessageLoopForIO loop_;
  scoped_refptr<net::HttpServer> server_;
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  GURL server_url_;
  WebSocketRequestResponse ws_request_response_;
  bool close_on_message_;
  bool quit_on_close_;
};

}  // namespace

TEST_F(WebSocketTest, CreateDestroy) {
  CloseListener listener(false);
  WebSocket sock(context_getter_, GURL("http://ok"), &listener);
}

TEST_F(WebSocketTest, Connect) {
  CloseListener listener(false);
  ASSERT_TRUE(CreateWebSocket(server_url_, &listener));
  quit_on_close_ = true;
  base::RunLoop run_loop;
  loop_.PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromSeconds(10));
  run_loop.Run();
}

TEST_F(WebSocketTest, ConnectNoServer) {
  CloseListener listener(false);
  ASSERT_FALSE(CreateWebSocket(GURL("ws://127.0.0.1:33333"), NULL));
}

TEST_F(WebSocketTest, Connect404) {
  ws_request_response_ = kNotFound;
  CloseListener listener(false);
  ASSERT_FALSE(CreateWebSocket(server_url_, NULL));
  quit_on_close_ = true;
  base::RunLoop run_loop;
  loop_.PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromSeconds(10));
  run_loop.Run();
}

TEST_F(WebSocketTest, ConnectServerClosesConn) {
  ws_request_response_ = kClose;
  CloseListener listener(false);
  ASSERT_FALSE(CreateWebSocket(server_url_, &listener));
}

TEST_F(WebSocketTest, CloseOnReceive) {
  close_on_message_ = true;
  CloseListener listener(true);
  scoped_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
  ASSERT_TRUE(sock);
  ASSERT_TRUE(sock->Send("hi"));
  base::RunLoop run_loop;
  loop_.PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromSeconds(10));
  run_loop.Run();
}

TEST_F(WebSocketTest, CloseOnSend) {
  CloseListener listener(true);
  scoped_ptr<WebSocket> sock(CreateConnectedWebSocket(&listener));
  ASSERT_TRUE(sock);
  server_ = NULL;
  loop_.PostTask(
      FROM_HERE,
      base::Bind(base::IgnoreResult(&WebSocket::Send),
                 base::Unretained(sock.get()), "hi"));
  base::RunLoop run_loop;
  loop_.PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromSeconds(10));
  run_loop.Run();
}

TEST_F(WebSocketTest, SendReceive) {
  std::vector<std::string> messages;
  messages.push_back("hello");
  SendReceive(messages);
}

TEST_F(WebSocketTest, SendReceiveLarge) {
  std::vector<std::string> messages;
  // Sends/receives 200kb. For some reason pushing this above 240kb on my
  // machine results in receiving no data back from the http server.
  messages.push_back(std::string(200 << 10, 'a'));
  SendReceive(messages);
}

TEST_F(WebSocketTest, SendReceiveMultiple) {
  std::vector<std::string> messages;
  messages.push_back("1");
  messages.push_back("2");
  messages.push_back("3");
  SendReceive(messages);
}
