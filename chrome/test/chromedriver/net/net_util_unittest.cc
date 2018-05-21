// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/net_util.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread.h"
#include "chrome/test/chromedriver/net/url_request_context_getter.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/server/http_server.h"
#include "services/network/public/cpp/server/http_server_request_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FetchUrlTest : public testing::Test,
                     public network::server::HttpServer::Delegate {
 public:
  FetchUrlTest()
      : io_thread_("io"),
        response_(kSendHello),
        scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
    CHECK(io_thread_.StartWithOptions(options));
    context_getter_ = new URLRequestContextGetter(io_thread_.task_runner());
  }

  void StartOnServerThread(base::WaitableEvent* event) {
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
    base::Optional<net::IPEndPoint> local_address;
    network::mojom::TCPServerSocketPtr server_socket;
    base::RunLoop run_loop;
    network_context_->CreateTCPServerSocket(
        net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0), 1 /* backlog */,
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS),
        mojo::MakeRequest(&server_socket),
        base::BindLambdaForTesting(
            [&](int result, const base::Optional<net::IPEndPoint>& local_addr) {
              net_error = result;
              local_address = local_addr;
              run_loop.Quit();
            }));
    run_loop.Run();
    EXPECT_EQ(net::OK, net_error);

    server_url_ = base::StringPrintf("http://[::1]:%d", local_address->port());

    server_.reset(
        new network::server::HttpServer(std::move(server_socket), this));
    event->Signal();
  }

  void StopOnServerThread(base::WaitableEvent* event) {
    server_.reset(nullptr);
    network_context_.reset(nullptr);
    network_service_.reset(nullptr);
    event->Signal();
  }

  void SetUp() override {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FetchUrlTest::StartOnServerThread,
                                  base::Unretained(this), &event));
    event.Wait();
  }

  void TearDown() override {
    if (!io_thread_.IsRunning())
      return;
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    io_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&FetchUrlTest::StopOnServerThread,
                                  base::Unretained(this), &event));
    event.Wait();
    io_thread_.Stop();
  }

  // Overridden from network::server::HttpServer::Delegate:
  void OnConnect(int connection_id) override {}

  void OnHttpRequest(
      int connection_id,
      const network::server::HttpServerRequestInfo& info) override {
    switch (response_) {
      case kSendHello:
        server_->Send200(connection_id, "hello", "text/plain",
                         TRAFFIC_ANNOTATION_FOR_TESTS);
        break;
      case kSend404:
        server_->Send404(connection_id, TRAFFIC_ANNOTATION_FOR_TESTS);
        break;
      case kClose:
        server_->Close(connection_id);
        break;
      default:
        break;
    }
  }

  void OnWebSocketRequest(
      int connection_id,
      const network::server::HttpServerRequestInfo& info) override {}
  void OnWebSocketMessage(int connection_id, const std::string& data) override {
  }
  void OnClose(int connection_id) override {}

 protected:
  enum ServerResponse {
    kSendHello = 0,
    kSend404,
    kClose,
  };

  base::Thread io_thread_;
  ServerResponse response_;
  std::unique_ptr<network::server::HttpServer> server_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<network::NetworkService> network_service_;
  std::unique_ptr<network::NetworkContext> network_context_;

  network::mojom::NetworkContextPtr network_context_ptr_;
  scoped_refptr<URLRequestContextGetter> context_getter_;
  std::string server_url_;
};

}  // namespace

TEST_F(FetchUrlTest, Http200) {
  response_ = kSendHello;
  std::string response("stuff");
  int response_code = -1;
  ASSERT_TRUE(
      FetchUrl(server_url_, context_getter_.get(), &response, &response_code));
  EXPECT_EQ(response_code, 200);
  ASSERT_STREQ("hello", response.c_str());
}

TEST_F(FetchUrlTest, HttpNon200) {
  response_ = kSend404;
  std::string response("stuff");
  int response_code = -1;
  ASSERT_FALSE(
      FetchUrl(server_url_, context_getter_.get(), &response, &response_code));
  EXPECT_EQ(response_code, 404);
  ASSERT_STREQ("stuff", response.c_str());
}

TEST_F(FetchUrlTest, ConnectionClose) {
  response_ = kClose;
  std::string response("stuff");
  int response_code = -1;
  ASSERT_FALSE(
      FetchUrl(server_url_, context_getter_.get(), &response, &response_code));
  EXPECT_LT(response_code, 0);
  ASSERT_STREQ("stuff", response.c_str());
}

TEST_F(FetchUrlTest, NoServer) {
  std::string response("stuff");
  int response_code = -1;
  ASSERT_FALSE(FetchUrl("http://localhost:33333", context_getter_.get(),
                        &response, &response_code));
  EXPECT_LT(response_code, 0);
  ASSERT_STREQ("stuff", response.c_str());
}
