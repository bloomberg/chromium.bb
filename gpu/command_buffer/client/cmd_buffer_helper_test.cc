// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Command Buffer Helper.

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::DoAll;
using testing::Invoke;
using testing::_;

const int32 kNumCommandEntries = 10;
const int32 kCommandBufferSizeBytes =
    kNumCommandEntries * sizeof(CommandBufferEntry);

// Test fixture for CommandBufferHelper test - Creates a CommandBufferHelper,
// using a CommandBufferEngine with a mock AsyncAPIInterface for its interface
// (calling it directly, not through the RPC mechanism).
class CommandBufferHelperTest : public testing::Test {
 protected:
  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(0, _, _))
        .WillRepeatedly(Return(parse_error::kParseNoError));

    command_buffer_.reset(new CommandBufferService);
    command_buffer_->Initialize(kNumCommandEntries);
    Buffer ring_buffer = command_buffer_->GetRingBuffer();

    parser_ = new CommandParser(ring_buffer.ptr,
                                ring_buffer.size,
                                0,
                                ring_buffer.size,
                                0,
                                api_mock_.get());

    scoped_refptr<GPUProcessor> gpu_processor(new GPUProcessor(
        command_buffer_.get(), NULL, parser_, 1));
    command_buffer_->SetPutOffsetChangeCallback(NewCallback(
        gpu_processor.get(), &GPUProcessor::ProcessCommands));

    api_mock_->set_engine(gpu_processor.get());

    helper_.reset(new CommandBufferHelper(command_buffer_.get()));
    helper_->Initialize();
  }

  virtual void TearDown() {
    // If the GPUProcessor posts any tasks, this forces them to run.
    MessageLoop::current()->RunAllPending();
    helper_.release();
  }

  // Adds a command to the buffer through the helper, while adding it as an
  // expected call on the API mock.
  void AddCommandWithExpect(parse_error::ParseError _return,
                            unsigned int command,
                            int arg_count,
                            CommandBufferEntry *args) {
    CommandHeader header;
    header.size = arg_count + 1;
    header.command = command;
    CommandBufferEntry* cmds = helper_->GetSpace(arg_count + 1);
    CommandBufferOffset put = 0;
    cmds[put++].value_header = header;
    for (int ii = 0; ii < arg_count; ++ii) {
      cmds[put++] = args[ii];
    }

    EXPECT_CALL(*api_mock_, DoCommand(command, arg_count,
        Truly(AsyncAPIMock::IsArgs(arg_count, args))))
        .InSequence(sequence_)
        .WillOnce(Return(_return));
  }

  // Checks that the buffer from put to put+size is free in the parser.
  void CheckFreeSpace(CommandBufferOffset put, unsigned int size) {
    CommandBufferOffset parser_put = parser_->put();
    CommandBufferOffset parser_get = parser_->get();
    CommandBufferOffset limit = put + size;
    if (parser_get > parser_put) {
      // "busy" buffer wraps, so "free" buffer is between put (inclusive) and
      // get (exclusive).
      EXPECT_LE(parser_put, put);
      EXPECT_GT(parser_get, limit);
    } else {
      // "busy" buffer does not wrap, so the "free" buffer is the top side (from
      // put to the limit) and the bottom side (from 0 to get).
      if (put >= parser_put) {
        // we're on the top side, check we are below the limit.
        EXPECT_GE(kNumCommandEntries, limit);
      } else {
        // we're on the bottom side, check we are below get.
        EXPECT_GT(parser_get, limit);
      }
    }
  }

  int32 GetGetOffset() {
    return command_buffer_->GetState().get_offset;
  }

  int32 GetPutOffset() {
    return command_buffer_->GetState().put_offset;
  }

  parse_error::ParseError GetError() {
    return command_buffer_->GetState().error;
  }

  CommandBufferOffset get_helper_put() { return helper_->put_; }

  base::AtExitManager at_exit_manager_;
  MessageLoop message_loop_;
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_ptr<CommandBufferService> command_buffer_;
  CommandParser* parser_;
  scoped_ptr<CommandBufferHelper> helper_;
  Sequence sequence_;
};

// Checks that commands in the buffer are properly executed, and that the
// status/error stay valid.
TEST_F(CommandBufferHelperTest, TestCommandProcessing) {
  // Check initial state of the engine - it should have been configured by the
  // helper.
  EXPECT_TRUE(parser_ != NULL);
  EXPECT_EQ(parse_error::kParseNoError, GetError());
  EXPECT_EQ(0, GetGetOffset());

  // Add 3 commands through the helper
  AddCommandWithExpect(parse_error::kParseNoError, 1, 0, NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  AddCommandWithExpect(parse_error::kParseNoError, 2, 2, args1);

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  AddCommandWithExpect(parse_error::kParseNoError, 3, 2, args2);

  helper_->Flush();
  // Check that the engine has work to do now.
  EXPECT_FALSE(parser_->IsEmpty());

  // Wait until it's done.
  helper_->Finish();
  // Check that the engine has no more work to do.
  EXPECT_TRUE(parser_->IsEmpty());

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(parse_error::kParseNoError, GetError());
}

// Checks that commands in the buffer are properly executed when wrapping the
// buffer, and that the status/error stay valid.
TEST_F(CommandBufferHelperTest, TestCommandWrapping) {
  // Add 5 commands of size 3 through the helper to make sure we do wrap.
  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;

  for (unsigned int i = 0; i < 5; ++i) {
    AddCommandWithExpect(parse_error::kParseNoError, i + 1, 2, args1);
  }

  helper_->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(parse_error::kParseNoError, GetError());
}

// Checks that asking for available entries work, and that the parser
// effectively won't use that space.
TEST_F(CommandBufferHelperTest, TestAvailableEntries) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add 2 commands through the helper - 8 entries
  AddCommandWithExpect(parse_error::kParseNoError, 1, 0, NULL);
  AddCommandWithExpect(parse_error::kParseNoError, 2, 0, NULL);
  AddCommandWithExpect(parse_error::kParseNoError, 3, 2, args);
  AddCommandWithExpect(parse_error::kParseNoError, 4, 2, args);

  // Ask for 5 entries.
  helper_->WaitForAvailableEntries(5);

  CommandBufferOffset put = get_helper_put();
  CheckFreeSpace(put, 5);

  // Add more commands.
  AddCommandWithExpect(parse_error::kParseNoError, 5, 2, args);

  // Wait until everything is done done.
  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(parse_error::kParseNoError, GetError());
}

// Checks that the InsertToken/WaitForToken work.
TEST_F(CommandBufferHelperTest, TestToken) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add a first command.
  AddCommandWithExpect(parse_error::kParseNoError, 3, 2, args);
  // keep track of the buffer position.
  CommandBufferOffset command1_put = get_helper_put();
  int32 token = helper_->InsertToken();

  EXPECT_CALL(*api_mock_.get(), DoCommand(cmd::kSetToken, 1, _))
      .WillOnce(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                      Return(parse_error::kParseNoError)));
  // Add another command.
  AddCommandWithExpect(parse_error::kParseNoError, 4, 2, args);
  helper_->WaitForToken(token);
  // check that the get pointer is beyond the first command.
  EXPECT_LE(command1_put, GetGetOffset());
  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(parse_error::kParseNoError, GetError());
}

}  // namespace gpu
