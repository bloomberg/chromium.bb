// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ceee/ie/broker/broker_rpc_client.h"
#include "ceee/ie/broker/broker_rpc_server.h"

#include <atlbase.h>
#include "broker_rpc_lib.h"  // NOLINT
#include "ceee/ie/broker/broker_rpc_utils.h"
#include "ceee/ie/common/ceee_module_util.h"
#include "ceee/testing/utils/mock_static.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::StrEq;
using testing::StrictMock;
using testing::Return;

namespace {

using testing::_;

MOCK_STATIC_CLASS_BEGIN(BrokerRpcMock)
  MOCK_STATIC_INIT_BEGIN(BrokerRpcMock)
    MOCK_STATIC_INIT(GetRpcEndPointAddress);
    MOCK_STATIC_INIT(BrokerRpcServer_FireEvent)
  MOCK_STATIC_INIT_END()
  MOCK_STATIC0(std::wstring, , GetRpcEndPointAddress);
  MOCK_STATIC3(void, , BrokerRpcServer_FireEvent, handle_t, const char*,
               const char*);
MOCK_STATIC_CLASS_END(BrokerRpcMock)

class BrokerRpcTest : public testing::Test {
 protected:
  virtual void SetUp() {
    EXPECT_CALL(broker_rpc_mock_, GetRpcEndPointAddress())
        .WillRepeatedly(Return(L"BrokerRpcTestEP"));
  }

  virtual void TearDown() {
  }

  BrokerRpcMock broker_rpc_mock_;
};

TEST_F(BrokerRpcTest, ConnectNoServer) {
  BrokerRpcClient client;
  ASSERT_FALSE(client.is_connected());
  ASSERT_FALSE(SUCCEEDED(client.Connect(false)));
  ASSERT_FALSE(client.is_connected());
}

TEST_F(BrokerRpcTest, Connect) {
  BrokerRpcServer server;
  ASSERT_FALSE(server.is_started());
  ASSERT_TRUE(server.Start());
  ASSERT_TRUE(server.is_started());
  BrokerRpcClient client;
  ASSERT_TRUE(SUCCEEDED(client.Connect(false)));
  ASSERT_TRUE(client.is_connected());
}

TEST_F(BrokerRpcTest, FireEvent) {
  BrokerRpcServer server;
  ASSERT_TRUE(server.Start());

  BrokerRpcClient client;
  ASSERT_TRUE(SUCCEEDED(client.Connect(false)));

  const char* event_name = "event_name";
  const char* event_args = "event_args";

  EXPECT_CALL(broker_rpc_mock_, BrokerRpcServer_FireEvent(_, StrEq(event_name),
                                                          StrEq(event_args)))
          .Times(1);

  ASSERT_TRUE(SUCCEEDED(client.FireEvent(event_name, event_args)));
}

}  // namespace
