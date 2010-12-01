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
    MOCK_STATIC_INIT(BrokerRpcServer_FireEvent);
    MOCK_STATIC_INIT(BrokerRpcServer_SendUmaHistogramTimes);
    MOCK_STATIC_INIT(BrokerRpcServer_SendUmaHistogramData);
  MOCK_STATIC_INIT_END()
  MOCK_STATIC0(std::wstring, , GetRpcEndPointAddress);
  MOCK_STATIC4(void, , BrokerRpcServer_FireEvent, handle_t, BrokerContextHandle,
               const char*, const char*);
  MOCK_STATIC3(void, , BrokerRpcServer_SendUmaHistogramTimes, handle_t,
               const char*, int);
  MOCK_STATIC6(void, , BrokerRpcServer_SendUmaHistogramData, handle_t,
               const char*, int, int, int, int);
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
  ASSERT_HRESULT_FAILED(client.Connect(false));
  ASSERT_FALSE(client.is_connected());
}

TEST_F(BrokerRpcTest, Connect) {
  BrokerRpcServer server;
  ASSERT_FALSE(server.is_started());
  ASSERT_TRUE(server.Start());
  ASSERT_TRUE(server.is_started());
  BrokerRpcClient client;
  ASSERT_HRESULT_SUCCEEDED(client.Connect(false));
  ASSERT_TRUE(client.is_connected());
}

TEST_F(BrokerRpcTest, RpcCalls) {
  BrokerRpcServer server;
  ASSERT_TRUE(server.Start());

  BrokerRpcClient client;
  ASSERT_HRESULT_SUCCEEDED(client.Connect(false));

  const char* name = "name";
  const char* args = "args";

  EXPECT_CALL(broker_rpc_mock_,
      BrokerRpcServer_FireEvent(_, _, StrEq(name), StrEq(args)))
          .Times(1);

  ASSERT_HRESULT_SUCCEEDED(client.FireEvent(name, args));

  EXPECT_CALL(broker_rpc_mock_,
      BrokerRpcServer_SendUmaHistogramTimes(_, StrEq(name), 321))
          .Times(1);
  ASSERT_HRESULT_SUCCEEDED(client.SendUmaHistogramTimes(name, 321));

  EXPECT_CALL(broker_rpc_mock_,
      BrokerRpcServer_SendUmaHistogramData(_, StrEq(name), 1, 2, 3, 4))
          .Times(1);
  ASSERT_HRESULT_SUCCEEDED(client.SendUmaHistogramData(name, 1, 2, 3, 4));
}

}  // namespace
