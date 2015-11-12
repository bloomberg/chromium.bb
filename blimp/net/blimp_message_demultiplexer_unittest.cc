// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit test for data encryption functions.

#include "blimp/net/blimp_message_demultiplexer.h"

#include "base/logging.h"
#include "blimp/net/test_common.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InvokeArgument;
using testing::Ref;
using testing::Return;
using testing::SaveArg;

namespace blimp {

class BlimpMessageDemultiplexerTest : public testing::Test {
 public:
  void SetUp() override {
    demux_.AddProcessor(BlimpMessage::INPUT, &receiver1_);
    demux_.AddProcessor(BlimpMessage::COMPOSITOR, &receiver2_);
  }

 protected:
  BlimpMessage input_msg_;
  BlimpMessage compositor_msg_;
  MockBlimpMessageProcessor receiver1_;
  MockBlimpMessageProcessor receiver2_;
  net::CompletionCallback captured_cb_;
  BlimpMessageDemultiplexer demux_;
};

TEST_F(BlimpMessageDemultiplexerTest, ProcessMessageOK) {
  EXPECT_CALL(receiver1_, ProcessMessage(Ref(input_msg_), _))
      .WillOnce(SaveArg<1>(&captured_cb_));
  input_msg_.set_type(BlimpMessage::INPUT);
  net::TestCompletionCallback cb;
  demux_.ProcessMessage(input_msg_, cb.callback());
  captured_cb_.Run(net::OK);
  EXPECT_EQ(net::OK, cb.WaitForResult());
}

TEST_F(BlimpMessageDemultiplexerTest, ProcessMessageFailed) {
  EXPECT_CALL(receiver2_, ProcessMessage(Ref(compositor_msg_), _))
      .WillOnce(SaveArg<1>(&captured_cb_));
  compositor_msg_.set_type(BlimpMessage::COMPOSITOR);
  net::TestCompletionCallback cb2;
  demux_.ProcessMessage(compositor_msg_, cb2.callback());
  captured_cb_.Run(net::ERR_FAILED);
  EXPECT_EQ(net::ERR_FAILED, cb2.WaitForResult());
}


}  // namespace blimp
