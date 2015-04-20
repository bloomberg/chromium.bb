// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "components/devtools_http_handler/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_target.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/server_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace devtools_http_handler {
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
    base::MessageLoop::current()->PostTask(FROM_HERE,
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
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&QuitFromHandlerThread, quit_closure_1_));
    return scoped_ptr<net::ServerSocket>();
  }
};

class DummyDelegate : public DevToolsHttpHandlerDelegate {
 public:
  std::string GetDiscoveryPageHTML() override { return std::string(); }

  std::string GetFrontendResource(const std::string& path) override {
    return std::string();
  }
};

class DummyManagerDelegate : public content::DevToolsManagerDelegate {
 public:
  ~DummyManagerDelegate() override {}

  void Inspect(content::BrowserContext* browser_context,
               content::DevToolsAgentHost* agent_host) override {}

  void DevToolsAgentStateChanged(content::DevToolsAgentHost* agent_host,
                                 bool attached) override {}

  base::DictionaryValue* HandleCommand(
      content::DevToolsAgentHost* agent_host,
      base::DictionaryValue* command) override {
    return NULL;
  }

  scoped_ptr<content::DevToolsTarget> CreateNewTarget(
      const GURL& url) override {
    return scoped_ptr<content::DevToolsTarget>();
  }

  void EnumerateTargets(TargetCallback callback) override {
    TargetList result;
    callback.Run(result);
  }

  std::string GetPageThumbnailData(const GURL& url) override {
    return std::string();
  }
};

}

class DevToolsHttpHandlerTest : public testing::Test {
 public:
  DevToolsHttpHandlerTest() : testing::Test() { }

 protected:
  DummyManagerDelegate manager_delegate_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
};

TEST_F(DevToolsHttpHandlerTest, TestStartStop) {
  base::RunLoop run_loop, run_loop_2;
  scoped_ptr<DevToolsHttpHandler::ServerSocketFactory> factory(
      new DummyServerSocketFactory(run_loop.QuitClosure(),
                                   run_loop_2.QuitClosure()));
  scoped_ptr<DevToolsHttpHandler> devtools_http_handler(
      new DevToolsHttpHandler(factory.Pass(),
                              std::string(),
                              new DummyDelegate(),
                              &manager_delegate_,
                              base::FilePath(),
                              base::FilePath(),
                              std::string(),
                              std::string()));
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
  scoped_ptr<DevToolsHttpHandler> devtools_http_handler(
      new DevToolsHttpHandler(factory.Pass(),
                              std::string(),
                              new DummyDelegate(),
                              &manager_delegate_,
                              base::FilePath(),
                              base::FilePath(),
                              std::string(),
                              std::string()));
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
  scoped_ptr<DevToolsHttpHandler> devtools_http_handler(
      new DevToolsHttpHandler(factory.Pass(),
                              std::string(),
                              new DummyDelegate(),
                              &manager_delegate_,
                              temp_dir.path(),
                              base::FilePath(),
                              std::string(),
                              std::string()));
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

}  // namespace devtools_http_handler
