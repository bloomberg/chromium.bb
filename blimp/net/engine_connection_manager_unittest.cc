// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/engine_connection_manager.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/test_common.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Eq;
using testing::Return;
using testing::SaveArg;

namespace blimp {

class EngineConnectionManagerTest : public testing::Test {
 public:
  EngineConnectionManagerTest()
      : manager_(new EngineConnectionManager(&connection_handler_)) {}

  ~EngineConnectionManagerTest() override {}

  std::unique_ptr<BlimpConnection> CreateConnection() {
    return base::MakeUnique<BlimpConnection>(
        base::MakeUnique<MessagePort>(base::MakeUnique<MockPacketReader>(),
                                      base::MakeUnique<MockPacketWriter>()));
  }

 protected:
  testing::StrictMock<MockConnectionHandler> connection_handler_;
  std::unique_ptr<EngineConnectionManager> manager_;
};

TEST_F(EngineConnectionManagerTest, ConnectionSucceeds) {
  std::unique_ptr<testing::StrictMock<MockTransport>> transport1(
      new testing::StrictMock<MockTransport>());
  std::unique_ptr<testing::StrictMock<MockTransport>> transport2(
      new testing::StrictMock<MockTransport>());

  net::CompletionCallback connect_cb_1;
  net::CompletionCallback connect_cb_2;
  EXPECT_CALL(*transport1, Connect(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&connect_cb_1));
  EXPECT_CALL(*transport2, Connect(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&connect_cb_2));

  std::unique_ptr<MessagePort> port1 =
      base::MakeUnique<MessagePort>(base::MakeUnique<MockPacketReader>(),
                                    base::MakeUnique<MockPacketWriter>());
  std::unique_ptr<MessagePort> port2 =
      base::MakeUnique<MessagePort>(base::MakeUnique<MockPacketReader>(),
                                    base::MakeUnique<MockPacketWriter>());
  EXPECT_CALL(connection_handler_, HandleConnectionPtr(_)).Times(2);

  EXPECT_CALL(*transport1, TakeMessagePortPtr())
      .WillOnce(Return(port1.release()));
  EXPECT_CALL(*transport2, TakeMessagePortPtr())
      .WillOnce(Return(port2.release()));

  ASSERT_TRUE(connect_cb_1.is_null());
  manager_->AddTransport(std::move(transport1));
  ASSERT_FALSE(connect_cb_1.is_null());

  ASSERT_TRUE(connect_cb_2.is_null());
  manager_->AddTransport(std::move(transport2));
  ASSERT_FALSE(connect_cb_2.is_null());

  base::ResetAndReturn(&connect_cb_1).Run(net::OK);
  base::ResetAndReturn(&connect_cb_2).Run(net::OK);
  ASSERT_FALSE(connect_cb_1.is_null());
  ASSERT_FALSE(connect_cb_2.is_null());
}

}  // namespace blimp
