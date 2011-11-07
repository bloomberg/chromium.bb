// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/communicator/xmpp_connection_generator.h"

#include "base/basictypes.h"
#include "jingle/notifier/communicator/connection_options.h"
#include "jingle/notifier/communicator/connection_settings.h"
#include "net/base/host_port_pair.h"
#include "net/base/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

using ::testing::_;
using ::testing::InvokeWithoutArgs;

class MockXmppConnectionGeneratorDelegate
    : public XmppConnectionGenerator::Delegate {
 public:
  MOCK_METHOD1(OnNewSettings, void(const ConnectionSettings&));
  MOCK_METHOD2(OnExhaustedSettings, void(bool, int));
};

const ServerInformation kXmppServers[] = {
  ServerInformation(net::HostPortPair("www.foo.com", 5222), true),
  ServerInformation(net::HostPortPair("www.bar.com", 8080), false),
  ServerInformation(net::HostPortPair("www.baz.com", 80), true),
};

class XmppConnectionGeneratorTest : public testing::Test {
 public:
  XmppConnectionGeneratorTest()
      : xmpp_connection_generator_(
          &delegate_,
          &mock_host_resolver_,
          &connection_options_,
          false /* try_ssltcp_first */,
          ServerList(kXmppServers,
                     kXmppServers + arraysize(kXmppServers))) {}

  virtual ~XmppConnectionGeneratorTest() {}

 protected:
  MockXmppConnectionGeneratorDelegate delegate_;
  net::MockHostResolver mock_host_resolver_;
  ConnectionOptions connection_options_;
  XmppConnectionGenerator xmpp_connection_generator_;
};

TEST_F(XmppConnectionGeneratorTest, DnsFailure) {
  MessageLoop message_loop;

  EXPECT_CALL(delegate_, OnNewSettings(_)).Times(0);
  EXPECT_CALL(delegate_, OnExhaustedSettings(_, _)).
      WillOnce(InvokeWithoutArgs(&message_loop, &MessageLoop::Quit));

  mock_host_resolver_.rules()->AddSimulatedFailure("*");
  xmpp_connection_generator_.StartGenerating();
  message_loop.Run();
}

TEST_F(XmppConnectionGeneratorTest, DnsFailureSynchronous) {
  MessageLoop message_loop;

  EXPECT_CALL(delegate_, OnNewSettings(_)).Times(0);
  EXPECT_CALL(delegate_, OnExhaustedSettings(_, _)).Times(1);

  mock_host_resolver_.set_synchronous_mode(true);
  mock_host_resolver_.rules()->AddSimulatedFailure("*");
  xmpp_connection_generator_.StartGenerating();
}

TEST_F(XmppConnectionGeneratorTest, ConnectionFailure) {
  MessageLoop message_loop;

  EXPECT_CALL(delegate_, OnNewSettings(_)).
      Times(5).
      WillRepeatedly(
          InvokeWithoutArgs(
              &xmpp_connection_generator_,
              &XmppConnectionGenerator::UseNextConnection));
  EXPECT_CALL(delegate_, OnExhaustedSettings(_, _)).
      WillOnce(InvokeWithoutArgs(&message_loop, &MessageLoop::Quit));

  xmpp_connection_generator_.StartGenerating();
  message_loop.Run();
}

TEST_F(XmppConnectionGeneratorTest, ConnectionFailureSynchronous) {
  MessageLoop message_loop;

  EXPECT_CALL(delegate_, OnNewSettings(_)).
      Times(5).
      WillRepeatedly(
          InvokeWithoutArgs(
              &xmpp_connection_generator_,
              &XmppConnectionGenerator::UseNextConnection));
  EXPECT_CALL(delegate_, OnExhaustedSettings(_, _)).Times(1);

  mock_host_resolver_.set_synchronous_mode(true);
  xmpp_connection_generator_.StartGenerating();
}

}  // namespace

}  // namespace notifier
