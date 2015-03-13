// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_permission_broker_client.h"
#include "chromeos/network/firewall_hole.h"
#include "dbus/file_descriptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::DBusThreadManager;
using chromeos::FirewallHole;
using testing::_;

namespace {

ACTION_TEMPLATE(InvokeCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(p1)) {
  ::std::tr1::get<k>(args).Run(p1);
}

class MockPermissionsBrokerClient : public chromeos::PermissionBrokerClient {
 public:
  MockPermissionsBrokerClient() {}
  ~MockPermissionsBrokerClient() override {}

  MOCK_METHOD1(Init, void(dbus::Bus* bus));
  MOCK_METHOD3(RequestPathAccess,
               void(const std::string& path,
                    int interface_id,
                    const ResultCallback& callback));
  MOCK_METHOD4(RequestTcpPortAccess,
               void(uint16 port,
                    const std::string& interface,
                    const dbus::FileDescriptor& lifeline_fd,
                    const ResultCallback& callback));
  MOCK_METHOD4(RequestUdpPortAccess,
               void(uint16 port,
                    const std::string& interface,
                    const dbus::FileDescriptor& lifeline_fd,
                    const ResultCallback& callback));
  MOCK_METHOD3(ReleaseTcpPort,
               void(uint16 port,
                    const std::string& interface,
                    const ResultCallback& callback));
  MOCK_METHOD3(ReleaseUdpPort,
               void(uint16 port,
                    const std::string& interface,
                    const ResultCallback& callback));
};

}  // namespace

class FirewallHoleTest : public testing::Test {
 public:
  FirewallHoleTest() {}
  ~FirewallHoleTest() override {}

  void SetUp() override {
    mock_permissions_broker_client_ = new MockPermissionsBrokerClient();
    DBusThreadManager::GetSetterForTesting()->SetPermissionBrokerClient(
        make_scoped_ptr(mock_permissions_broker_client_));
  }

  void TearDown() override { DBusThreadManager::Shutdown(); }

  void AssertOpenSuccess(scoped_ptr<FirewallHole> hole) {
    EXPECT_TRUE(hole.get() != nullptr);
    run_loop_.Quit();
  }

  void AssertOpenFailure(scoped_ptr<FirewallHole> hole) {
    EXPECT_TRUE(hole.get() == nullptr);
    run_loop_.Quit();
  }

 private:
  base::MessageLoopForUI message_loop_;

 protected:
  base::RunLoop run_loop_;
  MockPermissionsBrokerClient* mock_permissions_broker_client_ = nullptr;
};

TEST_F(FirewallHoleTest, GrantTcpPortAccess) {
  EXPECT_CALL(*mock_permissions_broker_client_,
              RequestTcpPortAccess(1234, "foo0", _, _))
      .WillOnce(InvokeCallback<3>(true));
  EXPECT_CALL(*mock_permissions_broker_client_, ReleaseTcpPort(1234, "foo0", _))
      .WillOnce(InvokeCallback<2>(true));

  FirewallHole::Open(
      FirewallHole::PortType::TCP, 1234, "foo0",
      base::Bind(&FirewallHoleTest::AssertOpenSuccess, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(FirewallHoleTest, DenyTcpPortAccess) {
  EXPECT_CALL(*mock_permissions_broker_client_,
              RequestTcpPortAccess(1234, "foo0", _, _))
      .WillOnce(InvokeCallback<3>(false));

  FirewallHole::Open(
      FirewallHole::PortType::TCP, 1234, "foo0",
      base::Bind(&FirewallHoleTest::AssertOpenFailure, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(FirewallHoleTest, GrantUdpPortAccess) {
  EXPECT_CALL(*mock_permissions_broker_client_,
              RequestUdpPortAccess(1234, "foo0", _, _))
      .WillOnce(InvokeCallback<3>(true));
  EXPECT_CALL(*mock_permissions_broker_client_, ReleaseUdpPort(1234, "foo0", _))
      .WillOnce(InvokeCallback<2>(true));

  FirewallHole::Open(
      FirewallHole::PortType::UDP, 1234, "foo0",
      base::Bind(&FirewallHoleTest::AssertOpenSuccess, base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(FirewallHoleTest, DenyUdpPortAccess) {
  EXPECT_CALL(*mock_permissions_broker_client_,
              RequestUdpPortAccess(1234, "foo0", _, _))
      .WillOnce(InvokeCallback<3>(false));

  FirewallHole::Open(
      FirewallHole::PortType::UDP, 1234, "foo0",
      base::Bind(&FirewallHoleTest::AssertOpenFailure, base::Unretained(this)));
  run_loop_.Run();
}
