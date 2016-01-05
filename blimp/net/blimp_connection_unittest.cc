// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>

#include "base/callback_helpers.h"
#include "base/message_loop/message_loop.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/net/blimp_connection.h"
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
using testing::InSequence;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace {

class BlimpConnectionTest : public testing::Test {
 public:
  BlimpConnectionTest() {
    scoped_ptr<testing::StrictMock<MockPacketWriter>> writer(
        new testing::StrictMock<MockPacketWriter>);
    writer_ = writer.get();
    connection_.reset(new BlimpConnection(make_scoped_ptr(new MockPacketReader),
                                          std::move(writer)));
    connection_->AddConnectionErrorObserver(&error_observer1_);
    connection_->AddConnectionErrorObserver(&error_observer2_);
    connection_->AddConnectionErrorObserver(&error_observer3_);
    connection_->RemoveConnectionErrorObserver(&error_observer3_);
  }

  ~BlimpConnectionTest() override {}

 protected:
  scoped_ptr<BlimpMessage> CreateInputMessage() {
    scoped_ptr<BlimpMessage> msg(new BlimpMessage);
    msg->set_type(BlimpMessage::INPUT);
    return msg;
  }

  scoped_ptr<BlimpMessage> CreateControlMessage() {
    scoped_ptr<BlimpMessage> msg(new BlimpMessage);
    msg->set_type(BlimpMessage::TAB_CONTROL);
    return msg;
  }

  base::MessageLoop message_loop_;
  testing::StrictMock<MockPacketWriter>* writer_;
  testing::StrictMock<MockConnectionErrorObserver> error_observer1_;
  testing::StrictMock<MockConnectionErrorObserver> error_observer2_;

  // This error observer is Removed() immediately after it's added;
  // it should never be called.
  testing::StrictMock<MockConnectionErrorObserver> error_observer3_;

  testing::StrictMock<MockBlimpMessageProcessor> receiver_;
  scoped_ptr<BlimpConnection> connection_;
};

// Write completes writing two packets asynchronously.
TEST_F(BlimpConnectionTest, AsyncTwoPacketsWrite) {
  net::CompletionCallback write_packet_cb;

  InSequence s;
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateInputMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateControlMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();

  BlimpMessageProcessor* sender = connection_->GetOutgoingMessageProcessor();
  net::TestCompletionCallback complete_cb_1;
  ASSERT_TRUE(write_packet_cb.is_null());
  sender->ProcessMessage(CreateInputMessage(),
                         complete_cb_1.callback());
  ASSERT_FALSE(write_packet_cb.is_null());
  base::ResetAndReturn(&write_packet_cb).Run(net::OK);
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());

  net::TestCompletionCallback complete_cb_2;
  ASSERT_TRUE(write_packet_cb.is_null());
  sender->ProcessMessage(CreateControlMessage(),
                         complete_cb_2.callback());
  ASSERT_FALSE(write_packet_cb.is_null());
  base::ResetAndReturn(&write_packet_cb).Run(net::OK);
  EXPECT_EQ(net::OK, complete_cb_2.WaitForResult());
}

// Writer completes writing two packets asynchronously.
// First write succeeds, second fails.
TEST_F(BlimpConnectionTest, AsyncTwoPacketsWriteWithError) {
  net::CompletionCallback write_packet_cb;

  InSequence s;
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateInputMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();
  EXPECT_CALL(*writer_,
              WritePacket(BufferEqualsProto(*CreateControlMessage()), _))
      .WillOnce(SaveArg<1>(&write_packet_cb))
      .RetiresOnSaturation();
  EXPECT_CALL(error_observer1_, OnConnectionError(net::ERR_FAILED));
  EXPECT_CALL(error_observer2_, OnConnectionError(net::ERR_FAILED));

  BlimpMessageProcessor* sender = connection_->GetOutgoingMessageProcessor();
  net::TestCompletionCallback complete_cb_1;
  sender->ProcessMessage(CreateInputMessage(),
                         complete_cb_1.callback());
  base::ResetAndReturn(&write_packet_cb).Run(net::OK);
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());

  net::TestCompletionCallback complete_cb_2;
  sender->ProcessMessage(CreateControlMessage(),
                         complete_cb_2.callback());
  base::ResetAndReturn(&write_packet_cb).Run(net::ERR_FAILED);
  EXPECT_EQ(net::ERR_FAILED, complete_cb_2.WaitForResult());
}

}  // namespace
}  // namespace blimp
