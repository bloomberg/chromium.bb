// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/api/dns/host_resolver_wrapper.h"
#include "chrome/browser/extensions/api/dns/mock_host_resolver_creator.h"
#include "chrome/browser/extensions/api/socket/socket_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"
#include "net/test/test_server.h"

using extensions::Extension;

namespace utils = extension_function_test_utils;

namespace {

const std::string kHostname = "127.0.0.1";
const int kPort = 8888;

class SocketApiTest : public PlatformAppApiTest {
 public:
  SocketApiTest() : resolver_event_(true, false),
                    resolver_creator_(
                        new extensions::MockHostResolverCreator()) {
  }

  void SetUpOnMainThread() OVERRIDE {
    extensions::HostResolverWrapper::GetInstance()->SetHostResolverForTesting(
        resolver_creator_->CreateMockHostResolver());
  }

  void CleanUpOnMainThread() OVERRIDE {
    extensions::HostResolverWrapper::GetInstance()->
        SetHostResolverForTesting(NULL);
    resolver_creator_->DeleteMockHostResolver();
  }

 private:
  base::WaitableEvent resolver_event_;

  // The MockHostResolver asserts that it's used on the same thread on which
  // it's created, which is actually a stronger rule than its real counterpart.
  // But that's fine; it's good practice.
  scoped_refptr<extensions::MockHostResolverCreator> resolver_creator_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketUDPCreateGood) {
  scoped_refptr<extensions::SocketCreateFunction> socket_create_function(
      new extensions::SocketCreateFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      socket_create_function,
      "[\"udp\"]",
      browser(), utils::NONE));
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  DictionaryValue *value = static_cast<DictionaryValue*>(result.get());
  int socketId = -1;
  EXPECT_TRUE(value->GetInteger("socketId", &socketId));
  EXPECT_TRUE(socketId > 0);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketTCPCreateGood) {
  scoped_refptr<extensions::SocketCreateFunction> socket_create_function(
      new extensions::SocketCreateFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      socket_create_function,
      "[\"tcp\"]",
      browser(), utils::NONE));
  ASSERT_EQ(base::Value::TYPE_DICTIONARY, result->GetType());
  DictionaryValue *value = static_cast<DictionaryValue*>(result.get());
  int socketId = -1;
  EXPECT_TRUE(value->GetInteger("socketId", &socketId));
  ASSERT_TRUE(socketId > 0);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketCreateBad) {
  scoped_refptr<extensions::SocketCreateFunction> socket_create_function(
      new extensions::SocketCreateFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  socket_create_function->set_extension(empty_extension.get());
  socket_create_function->set_has_callback(true);

  // TODO(miket): this test currently passes only because of artificial code
  // that doesn't run in production. Fix this when we're able to.
  utils::RunFunctionAndReturnError(
      socket_create_function,
      "[\"xxxx\"]",
      browser(), utils::NONE);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, GetNetworkList) {
  scoped_refptr<extensions::SocketGetNetworkListFunction> socket_function(
      new extensions::SocketGetNetworkListFunction());
  scoped_refptr<Extension> empty_extension(utils::CreateEmptyExtension());

  socket_function->set_extension(empty_extension.get());
  socket_function->set_has_callback(true);

  scoped_ptr<base::Value> result(utils::RunFunctionAndReturnSingleResult(
      socket_function, "[]", browser(), utils::NONE));
  ASSERT_EQ(base::Value::TYPE_LIST, result->GetType());

  // If we're invoking socket tests, all we can confirm is that we have at
  // least one address, but not what it is.
  ListValue *value = static_cast<ListValue*>(result.get());
  ASSERT_TRUE(value->GetSize() > 0);
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketUDPExtension) {
  scoped_ptr<net::TestServer> test_server(
      new net::TestServer(net::TestServer::TYPE_UDP_ECHO,
                          net::TestServer::kLocalhost,
                          FilePath(FILE_PATH_LITERAL("net/data"))));
  EXPECT_TRUE(test_server->Start());

  net::HostPortPair host_port_pair = test_server->host_port_pair();
  int port = host_port_pair.port();
  ASSERT_TRUE(port > 0);

  // Test that sendTo() is properly resolving hostnames.
  host_port_pair.set_host("LOCALhost");

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener listener("info_please", true);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("udp:%s:%d", host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(SocketApiTest, SocketTCPExtension) {
  scoped_ptr<net::TestServer> test_server(
      new net::TestServer(net::TestServer::TYPE_TCP_ECHO,
                          net::TestServer::kLocalhost,
                          FilePath(FILE_PATH_LITERAL("net/data"))));
  EXPECT_TRUE(test_server->Start());

  net::HostPortPair host_port_pair = test_server->host_port_pair();
  int port = host_port_pair.port();
  ASSERT_TRUE(port > 0);

  // Test that connect() is properly resolving hostnames.
  host_port_pair.set_host("lOcAlHoSt");

  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener listener("info_please", true);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("socket/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(
      base::StringPrintf("tcp:%s:%d", host_port_pair.host().c_str(), port));

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
