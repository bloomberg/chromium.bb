// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "blimp/net/blimp_connection.h"
#include "blimp/net/blimp_transport.h"
#include "blimp/net/common.h"
#include "blimp/net/connection_error_observer.h"
#include "blimp/net/engine_connection_manager.h"
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

  scoped_ptr<BlimpConnection> CreateConnection() {
    return make_scoped_ptr(
        new BlimpConnection(make_scoped_ptr(new MockPacketReader),
                            make_scoped_ptr(new MockPacketWriter)));
  }

 protected:
  base::MessageLoopForIO message_loop_;
  testing::StrictMock<MockConnectionHandler> connection_handler_;
  scoped_ptr<EngineConnectionManager> manager_;
};

TEST_F(EngineConnectionManagerTest, ConnectionSucceeds) {
  scoped_ptr<testing::StrictMock<MockTransport>> transport1(
      new testing::StrictMock<MockTransport>);
  scoped_ptr<testing::StrictMock<MockTransport>> transport2(
      new testing::StrictMock<MockTransport>);

  scoped_ptr<BlimpConnection> connection1 = CreateConnection();
  net::CompletionCallback connect_cb_1;
  EXPECT_CALL(*transport1, Connect(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&connect_cb_1));
  EXPECT_CALL(connection_handler_, HandleConnectionPtr(Eq(connection1.get())));
  EXPECT_CALL(*transport1, TakeConnectionPtr())
      .WillOnce(Return(connection1.release()));

  scoped_ptr<BlimpConnection> connection2 = CreateConnection();
  net::CompletionCallback connect_cb_2;
  EXPECT_CALL(*transport2, Connect(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&connect_cb_2));
  EXPECT_CALL(connection_handler_, HandleConnectionPtr(Eq(connection2.get())));
  EXPECT_CALL(*transport2, TakeConnectionPtr())
      .WillOnce(Return(connection2.release()));

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
