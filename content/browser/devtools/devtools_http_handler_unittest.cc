// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/browser_thread_impl.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/test/test_utils.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

const uint16 kDummyPort = 4321;
const base::FilePath::CharType kDevToolsActivePortFileName[] =
    FILE_PATH_LITERAL("DevToolsActivePort");

class DummyServerSocket : public net::ServerSocket {
 public:
  DummyServerSocket() {}

  // net::ServerSocket "implementation"
  int Listen(const net::IPEndPoint& address, int backlog) override {
    return net::OK;
  }

  int GetLocalAddress(net::IPEndPoint* address) const override {
    net::IPAddressNumber number;
    EXPECT_TRUE(net::ParseIPLiteralToNumber("127.0.0.1", &number));
    *address = net::IPEndPoint(number, kDummyPort);
    return net::OK;
  }

  int Accept(scoped_ptr<net::StreamSocket>* socket,
             const net::CompletionCallback& callback) override {
    return net::ERR_IO_PENDING;
  }
};

void QuitFromHandlerThread(const base::Closure& quit_closure) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, quit_closure);
}

class DummyServerSocketFactory
    : public DevToolsHttpHandler::ServerSocketFactory {
 public:
  DummyServerSocketFactory(base::Closure quit_closure_1,
                           base::Closure quit_closure_2)
      : quit_closure_1_(quit_closure_1),
        quit_closure_2_(quit_closure_2) {}

  ~DummyServerSocketFactory() override {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, quit_closure_2_);
  }

 protected:
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(&QuitFromHandlerThread, quit_closure_1_));
    return scoped_ptr<net::ServerSocket>(new DummyServerSocket());
  }

  base::Closure quit_closure_1_;
  base::Closure quit_closure_2_;
};

class FailingServerSocketFactory : public DummyServerSocketFactory {
 public:
  FailingServerSocketFactory(const base::Closure& quit_closure_1,
                             const base::Closure& quit_closure_2)
      : DummyServerSocketFactory(quit_closure_1, quit_closure_2) {
  }

 private:
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
        base::Bind(&QuitFromHandlerThread, quit_closure_1_));
    return scoped_ptr<net::ServerSocket>();
  }
};

class DummyDelegate : public DevToolsHttpHandlerDelegate {
 public:
  std::string GetDiscoveryPageHTML() override { return std::string(); }

  bool BundlesFrontendResources() override { return true; }

  base::FilePath GetDebugFrontendDir() override { return base::FilePath(); }
};

}

class DevToolsHttpHandlerTest : public testing::Test {
 public:
  DevToolsHttpHandlerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  void SetUp() override {
    file_thread_.reset(new BrowserThreadImpl(BrowserThread::FILE));
    file_thread_->Start();
  }

  void TearDown() override { file_thread_->Stop(); }

 private:
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl ui_thread_;
  scoped_ptr<BrowserThreadImpl> file_thread_;
};

TEST_F(DevToolsHttpHandlerTest, TestStartStop) {
  base::RunLoop run_loop, run_loop_2;
  scoped_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(
      new DummyServerSocketFactory(run_loop.QuitClosure(),
                                   run_loop_2.QuitClosure()));
  scoped_ptr<content::DevToolsHttpHandler> devtools_http_handler(
      content::DevToolsHttpHandler::Start(factory.Pass(),
                                          std::string(),
                                          new DummyDelegate(),
                                          base::FilePath()));
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  devtools_http_handler.reset();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}

TEST_F(DevToolsHttpHandlerTest, TestServerSocketFailed) {
  base::RunLoop run_loop, run_loop_2;
  scoped_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(
      new FailingServerSocketFactory(run_loop.QuitClosure(),
                                     run_loop_2.QuitClosure()));
  scoped_ptr<content::DevToolsHttpHandler> devtools_http_handler(
      content::DevToolsHttpHandler::Start(factory.Pass(),
                                          std::string(),
                                          new DummyDelegate(),
                                          base::FilePath()));
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  for (int i = 0; i < 5; i++) {
    RunAllPendingInMessageLoop(BrowserThread::UI);
    RunAllPendingInMessageLoop(BrowserThread::FILE);
  }
  devtools_http_handler.reset();
  // Make sure the handler actually stops.
  run_loop_2.Run();
}


TEST_F(DevToolsHttpHandlerTest, TestDevToolsActivePort) {
  base::RunLoop run_loop, run_loop_2;
  base::ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  scoped_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(
      new DummyServerSocketFactory(run_loop.QuitClosure(),
                                   run_loop_2.QuitClosure()));
  scoped_ptr<content::DevToolsHttpHandler> devtools_http_handler(
      content::DevToolsHttpHandler::Start(factory.Pass(),
                                          std::string(),
                                          new DummyDelegate(),
                                          temp_dir.path()));
  // Our dummy socket factory will post a quit message once the server will
  // become ready.
  run_loop.Run();
  devtools_http_handler.reset();
  // Make sure the handler actually stops.
  run_loop_2.Run();

  // Now make sure the DevToolsActivePort was written into the
  // temporary directory and its contents are as expected.
  base::FilePath active_port_file = temp_dir.path().Append(
      kDevToolsActivePortFileName);
  EXPECT_TRUE(base::PathExists(active_port_file));
  std::string file_contents;
  EXPECT_TRUE(base::ReadFileToString(active_port_file, &file_contents));
  int port = 0;
  EXPECT_TRUE(base::StringToInt(file_contents, &port));
  EXPECT_EQ(static_cast<int>(kDummyPort), port);
}

}  // namespace content
