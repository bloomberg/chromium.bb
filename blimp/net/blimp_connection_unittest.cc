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
using testing::DoAll;
using testing::InSequence;
using testing::NotNull;
using testing::Return;
using testing::SaveArg;

namespace blimp {
namespace {

class BlimpConnectionTest : public testing::Test {
 public:
  BlimpConnectionTest() {
    message1_ = CreateInputMessage();
    message2_ = CreateControlMessage();
    scoped_ptr<testing::StrictMock<MockPacketReader>> reader(
        new testing::StrictMock<MockPacketReader>);
    reader_ = reader.get();
    scoped_ptr<testing::StrictMock<MockPacketWriter>> writer(
        new testing::StrictMock<MockPacketWriter>);
    writer_ = writer.get();
    connection_.reset(new BlimpConnection(std::move(reader),
                                          std::move(writer)));
    connection_->SetConnectionErrorObserver(&error_observer_);
  }

  ~BlimpConnectionTest() override {}

  scoped_ptr<BlimpMessage> CreateInputMessage() {
    scoped_ptr<BlimpMessage> msg(new BlimpMessage);
    msg->set_type(BlimpMessage::INPUT);
    return msg;
  }

  scoped_ptr<BlimpMessage> CreateControlMessage() {
    scoped_ptr<BlimpMessage> msg(new BlimpMessage);
    msg->set_type(BlimpMessage::CONTROL);
    return msg;
  }

 protected:
  base::MessageLoopForIO message_loop_;
  scoped_ptr<BlimpMessage> message1_;
  scoped_ptr<BlimpMessage> message2_;

  testing::StrictMock<MockPacketReader>* reader_;
  testing::StrictMock<MockPacketWriter>* writer_;
  testing::StrictMock<MockConnectionErrorObserver> error_observer_;
  testing::StrictMock<MockBlimpMessageProcessor> receiver_;
  scoped_ptr<BlimpConnection> connection_;
};

// Reader completes reading one packet synchronously.
// Two read cases here. BlimpMessagePumpTest covers other cases.
TEST_F(BlimpConnectionTest, SyncPacketRead) {
  EXPECT_CALL(receiver_, MockableProcessMessage(EqualsProto(*message1_), _));
  EXPECT_CALL(*reader_, ReadPacket(NotNull(), _))
      .WillOnce(DoAll(FillBufferFromMessage<0>(message1_.get()),
                      Return(message1_->ByteSize())));
  connection_->SetIncomingMessageProcessor(&receiver_);
}

// Reader completes reading one packet synchronously withe error.
TEST_F(BlimpConnectionTest, SyncPacketReadWithError) {
  InSequence s;
  EXPECT_CALL(*reader_, ReadPacket(NotNull(), _))
      .WillOnce(Return(net::ERR_FAILED));
  EXPECT_CALL(error_observer_, OnConnectionError(net::ERR_FAILED));
  connection_->SetIncomingMessageProcessor(&receiver_);
}

// Writer completes writing two packets synchronously.
TEST_F(BlimpConnectionTest, SyncTwoPacketsWrite) {
  InSequence s;
  EXPECT_CALL(*writer_, WritePacket(BufferEqualsProto(*message1_), _))
      .WillOnce(Return(net::OK))
      .RetiresOnSaturation();
  EXPECT_CALL(*writer_, WritePacket(BufferEqualsProto(*message2_), _))
      .WillOnce(Return(net::OK))
      .RetiresOnSaturation();

  BlimpMessageProcessor* sender = connection_->GetOutgoingMessageProcessor();
  net::TestCompletionCallback complete_cb_1;
  sender->ProcessMessage(CreateInputMessage(),
                         complete_cb_1.callback());
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());
  net::TestCompletionCallback complete_cb_2;
  sender->ProcessMessage(CreateControlMessage(),
                         complete_cb_2.callback());
  EXPECT_EQ(net::OK, complete_cb_2.WaitForResult());
}

// Writer completes writing two packets synchronously.
// First write succeeds, second fails.
TEST_F(BlimpConnectionTest, SyncTwoPacketsWriteWithError) {
  InSequence s;
  EXPECT_CALL(*writer_, WritePacket(BufferEqualsProto(*message1_), _))
      .WillOnce(Return(net::OK))
      .RetiresOnSaturation();
  EXPECT_CALL(*writer_, WritePacket(BufferEqualsProto(*message2_), _))
      .WillOnce(Return(net::ERR_FAILED))
      .RetiresOnSaturation();
  EXPECT_CALL(error_observer_, OnConnectionError(net::ERR_FAILED));

  BlimpMessageProcessor* sender = connection_->GetOutgoingMessageProcessor();
  net::TestCompletionCallback complete_cb_1;
  sender->ProcessMessage(CreateInputMessage(),
                         complete_cb_1.callback());
  EXPECT_EQ(net::OK, complete_cb_1.WaitForResult());
  net::TestCompletionCallback complete_cb_2;
  sender->ProcessMessage(CreateControlMessage(),
                         complete_cb_2.callback());
  EXPECT_EQ(net::ERR_FAILED, complete_cb_2.WaitForResult());
}

// Write completes writing two packets asynchronously.
TEST_F(BlimpConnectionTest, AsyncTwoPacketsWrite) {
  net::CompletionCallback write_packet_cb;

  InSequence s;
  EXPECT_CALL(*writer_, WritePacket(BufferEqualsProto(*message1_), _))
      .WillOnce(
          DoAll(SaveArg<1>(&write_packet_cb), Return(net::ERR_IO_PENDING)))
      .RetiresOnSaturation();
  EXPECT_CALL(*writer_, WritePacket(BufferEqualsProto(*message2_), _))
      .WillOnce(
          DoAll(SaveArg<1>(&write_packet_cb), Return(net::ERR_IO_PENDING)))
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
  EXPECT_CALL(*writer_, WritePacket(BufferEqualsProto(*message1_), _))
      .WillOnce(
          DoAll(SaveArg<1>(&write_packet_cb), Return(net::ERR_IO_PENDING)))
      .RetiresOnSaturation();
  EXPECT_CALL(*writer_, WritePacket(BufferEqualsProto(*message2_), _))
      .WillOnce(
          DoAll(SaveArg<1>(&write_packet_cb), Return(net::ERR_IO_PENDING)))
      .RetiresOnSaturation();
  EXPECT_CALL(error_observer_, OnConnectionError(net::ERR_FAILED));

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
