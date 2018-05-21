// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/test_http_server.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/server/http_server_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

const int kBufferSize = 100 * 1024 * 1024;  // 100 MB

TestHttpServer::TestHttpServer()
    : thread_("ServerThread"),
      all_closed_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::SIGNALED),
      request_action_(kAccept),
      message_action_(kEchoMessage) {}

TestHttpServer::~TestHttpServer() {
}

bool TestHttpServer::Start() {
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  bool thread_started = thread_.StartWithOptions(options);
  EXPECT_TRUE(thread_started);
  if (!thread_started)
    return false;
  bool success;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestHttpServer::StartOnServerThread,
                                base::Unretained(this), &success, &event));
  event.Wait();
  return success;
}

void TestHttpServer::Stop() {
  if (!thread_.IsRunning())
    return;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&TestHttpServer::StopOnServerThread,
                                base::Unretained(this), &event));
  event.Wait();
  thread_.Stop();
}

bool TestHttpServer::WaitForConnectionsToClose() {
  return all_closed_event_.TimedWait(base::TimeDelta::FromSeconds(10));
}

void TestHttpServer::SetRequestAction(WebSocketRequestAction action) {
  base::AutoLock lock(action_lock_);
  request_action_ = action;
}

void TestHttpServer::SetMessageAction(WebSocketMessageAction action) {
  base::AutoLock lock(action_lock_);
  message_action_ = action;
}

void TestHttpServer::SetMessageCallback(const base::Closure& callback) {
  base::AutoLock lock(action_lock_);
  message_callback_ = callback;
}

GURL TestHttpServer::web_socket_url() const {
  base::AutoLock lock(url_lock_);
  return web_socket_url_;
}

void TestHttpServer::OnConnect(int connection_id) {
  server_->SetSendBufferSize(connection_id, kBufferSize);
  server_->SetReceiveBufferSize(connection_id, kBufferSize);
}

void TestHttpServer::OnWebSocketRequest(
    int connection_id,
    const network::server::HttpServerRequestInfo& info) {
  WebSocketRequestAction action;
  {
    base::AutoLock lock(action_lock_);
    action = request_action_;
  }
  connections_.insert(connection_id);
  all_closed_event_.Reset();

  switch (action) {
    case kAccept:
      server_->AcceptWebSocket(connection_id, info,
                               TRAFFIC_ANNOTATION_FOR_TESTS);
      break;
    case kNotFound:
      server_->Send404(connection_id, TRAFFIC_ANNOTATION_FOR_TESTS);
      break;
    case kClose:
      server_->Close(connection_id);
      break;
  }
}

void TestHttpServer::OnWebSocketMessage(int connection_id,
                                        const std::string& data) {
  WebSocketMessageAction action;
  base::Closure callback;
  {
    base::AutoLock lock(action_lock_);
    action = message_action_;
    callback = base::ResetAndReturn(&message_callback_);
  }
  if (!callback.is_null())
    callback.Run();
  switch (action) {
    case kEchoMessage:
      server_->SendOverWebSocket(connection_id, data,
                                 TRAFFIC_ANNOTATION_FOR_TESTS);
      break;
    case kCloseOnMessage:
      server_->Close(connection_id);
      break;
  }
}

void TestHttpServer::OnClose(int connection_id) {
  connections_.erase(connection_id);
  if (connections_.empty())
    all_closed_event_.Signal();
}

void TestHttpServer::StartOnServerThread(bool* success,
                                         base::WaitableEvent* event) {
  network_service_ = network::NetworkService::CreateForTesting();

  network::mojom::NetworkContextParamsPtr context_params =
      network::mojom::NetworkContextParams::New();
  // Use a fixed proxy config, to avoid dependencies on local network
  // configuration.
  context_params->initial_proxy_config =
      net::ProxyConfigWithAnnotation::CreateDirect();

  network_context_ = std::make_unique<network::NetworkContext>(
      network_service_.get(), mojo::MakeRequest(&network_context_ptr_),
      std::move(context_params));

  int net_error = net::ERR_FAILED;
  net::IPEndPoint address;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  network::mojom::TCPServerSocketPtr server_socket;
  network_context_ptr_->CreateTCPServerSocket(
      net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0), 1 /* backlog */,
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
      mojo::MakeRequest(&server_socket),
      base::BindLambdaForTesting(
          [&](int result, const base::Optional<net::IPEndPoint>& local_addr) {
            net_error = result;
            address = local_addr.value();
            run_loop.Quit();
          }));
  run_loop.Run();
  EXPECT_EQ(net::OK, net_error);

  server_ = std::make_unique<network::server::HttpServer>(
      std::move(server_socket), this);

  if (net_error == net::OK) {
    base::AutoLock lock(url_lock_);
    web_socket_url_ = GURL(base::StringPrintf("ws://[::1]:%d", address.port()));
  } else {
    server_.reset(nullptr);
  }
  *success = server_.get();
  event->Signal();
}

void TestHttpServer::StopOnServerThread(base::WaitableEvent* event) {
  server_ = nullptr;
  network_context_ptr_.reset();
  network_context_ = nullptr;
  network_service_ = nullptr;
  event->Signal();
}
