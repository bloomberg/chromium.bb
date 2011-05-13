// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/proxy_resolving_client_socket.h"

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// TODO(sanjeevr): Move this to net_test_support.
// Used to return a dummy context.
class TestURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestURLRequestContextGetter()
      : message_loop_proxy_(base::MessageLoopProxy::CreateForCurrentThread()) {
  }
  virtual ~TestURLRequestContextGetter() { }

  // net::URLRequestContextGetter:
  virtual net::URLRequestContext* GetURLRequestContext() {
    if (!context_)
      context_ = new TestURLRequestContext();
    return context_.get();
  }
  virtual scoped_refptr<base::MessageLoopProxy> GetIOMessageLoopProxy() const {
    return message_loop_proxy_;
  }

 private:
  scoped_refptr<net::URLRequestContext> context_;
  scoped_refptr<base::MessageLoopProxy> message_loop_proxy_;
};
}  // namespace

namespace notifier {

class ProxyResolvingClientSocketTest : public testing::Test {
 protected:
  ProxyResolvingClientSocketTest()
      : url_request_context_getter_(new TestURLRequestContextGetter()),
        connect_callback_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                          &ProxyResolvingClientSocketTest::NetCallback) { }

  virtual ~ProxyResolvingClientSocketTest() {}

  virtual void TearDown() {
    // Clear out any messages posted by ProxyResolvingClientSocket's
    // destructor.
    message_loop_.RunAllPending();
  }

  MOCK_METHOD1(NetCallback, void(int status));

  // Needed by XmppConnection.
  MessageLoopForIO message_loop_;
  scoped_refptr<TestURLRequestContextGetter> url_request_context_getter_;
  net::CompletionCallbackImpl<ProxyResolvingClientSocketTest> connect_callback_;
};

// TODO(sanjeevr): Fix this test on Linux.
TEST_F(ProxyResolvingClientSocketTest, DISABLED_ConnectError) {
  net::HostPortPair dest("0.0.0.0", 0);
  ProxyResolvingClientSocket proxy_resolving_socket(
      url_request_context_getter_,
      net::SSLConfig(),
      dest);
  // ProxyResolvingClientSocket::Connect() will always return an error of
  // ERR_ADDRESS_INVALID for a 0 IP address.
  EXPECT_CALL(*this, NetCallback(net::ERR_ADDRESS_INVALID)).Times(1);
  int status = proxy_resolving_socket.Connect(&connect_callback_);
  // Connect always returns ERR_IO_PENDING because it is always asynchronous.
  EXPECT_EQ(status, net::ERR_IO_PENDING);
  message_loop_.RunAllPending();
}

// TODO(sanjeevr): Add more unit-tests.
}  // namespace notifier
