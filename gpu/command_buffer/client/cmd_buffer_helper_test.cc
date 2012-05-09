// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Command Buffer Helper.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/message_loop.h"
#include "gpu/command_buffer/client/cmd_buffer_helper.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace gpu {

using testing::Return;
using testing::Mock;
using testing::Truly;
using testing::Sequence;
using testing::DoAll;
using testing::Invoke;
using testing::_;

const int32 kTotalNumCommandEntries = 12;
const int32 kUsableNumCommandEntries = 10;
const int32 kCommandBufferSizeBytes =
    kTotalNumCommandEntries * sizeof(CommandBufferEntry);
const int32 kUnusedCommandId = 5;  // we use 0 and 2 currently.

// Test fixture for CommandBufferHelper test - Creates a CommandBufferHelper,
// using a CommandBufferEngine with a mock AsyncAPIInterface for its interface
// (calling it directly, not through the RPC mechanism).
class CommandBufferHelperTest : public testing::Test {
 protected:
  // Helper so mock can handle the Jump command.
  class DoJumpCommand {
   public:
    explicit DoJumpCommand(GpuScheduler* gpu_scheduler)
        : gpu_scheduler_(gpu_scheduler) {
    }

    error::Error DoCommand(
        unsigned int command,
        unsigned int arg_count,
        const void* cmd_data) {
      const cmd::Jump* jump_cmd = static_cast<const cmd::Jump*>(cmd_data);
      gpu_scheduler_->parser()->set_get(jump_cmd->offset);
      return error::kNoError;
    };

   private:
    GpuScheduler* gpu_scheduler_;
  };

  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(cmd::kNoop, _, _))
        .WillRepeatedly(Return(error::kNoError));

    command_buffer_.reset(new CommandBufferService);
    command_buffer_->Initialize();

    gpu_scheduler_.reset(new GpuScheduler(
        command_buffer_.get(), api_mock_.get(), NULL));
    command_buffer_->SetPutOffsetChangeCallback(base::Bind(
        &GpuScheduler::PutChanged, base::Unretained(gpu_scheduler_.get())));
    command_buffer_->SetGetBufferChangeCallback(base::Bind(
        &GpuScheduler::SetGetBuffer, base::Unretained(gpu_scheduler_.get())));

    do_jump_command_.reset(new DoJumpCommand(gpu_scheduler_.get()));
    EXPECT_CALL(*api_mock_, DoCommand(cmd::kJump, _, _))
        .WillRepeatedly(
            Invoke(do_jump_command_.get(), &DoJumpCommand::DoCommand));


    api_mock_->set_engine(gpu_scheduler_.get());

    helper_.reset(new CommandBufferHelper(command_buffer_.get()));
    helper_->Initialize(kCommandBufferSizeBytes);
  }

  virtual void TearDown() {
    // If the GpuScheduler posts any tasks, this forces them to run.
    MessageLoop::current()->RunAllPending();
  }

  const CommandParser* GetParser() const {
    return gpu_scheduler_->parser();
  }

  // Adds a command to the buffer through the helper, while adding it as an
  // expected call on the API mock.
  void AddCommandWithExpect(error::Error _return,
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
    CommandBufferOffset parser_put = GetParser()->put();
    CommandBufferOffset parser_get = GetParser()->get();
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
        EXPECT_GE(kTotalNumCommandEntries, limit);
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

  error::Error GetError() {
    return command_buffer_->GetState().error;
  }

  CommandBufferOffset get_helper_put() { return helper_->put_; }

#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool_;
#endif
  MessageLoop message_loop_;
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_ptr<CommandBufferService> command_buffer_;
  scoped_ptr<GpuScheduler> gpu_scheduler_;
  scoped_ptr<CommandBufferHelper> helper_;
  Sequence sequence_;
  scoped_ptr<DoJumpCommand> do_jump_command_;
};

// Checks that commands in the buffer are properly executed, and that the
// status/error stay valid.
TEST_F(CommandBufferHelperTest, TestCommandProcessing) {
  // Check initial state of the engine - it should have been configured by the
  // helper.
  EXPECT_TRUE(GetParser() != NULL);
  EXPECT_EQ(error::kNoError, GetError());
  EXPECT_EQ(0, GetGetOffset());

  // Add 3 commands through the helper
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 0, NULL);

  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 3;
  args1[1].value_float = 4.f;
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 2, args1);

  CommandBufferEntry args2[2];
  args2[0].value_uint32 = 5;
  args2[1].value_float = 6.f;
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 2, args2);

  // Wait until it's done.
  helper_->Finish();
  // Check that the engine has no more work to do.
  EXPECT_TRUE(GetParser()->IsEmpty());

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks that commands in the buffer are properly executed when wrapping the
// buffer, and that the status/error stay valid.
TEST_F(CommandBufferHelperTest, TestCommandWrapping) {
  // Add 5 commands of size 3 through the helper to make sure we do wrap.
  CommandBufferEntry args1[2];
  args1[0].value_uint32 = 5;
  args1[1].value_float = 4.f;

  for (unsigned int i = 0; i < 5; ++i) {
    AddCommandWithExpect(error::kNoError, kUnusedCommandId + i, 2, args1);
  }

  helper_->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks the case where the command inserted exactly matches the space left in
// the command buffer.
TEST_F(CommandBufferHelperTest, TestCommandWrappingExactMultiple) {
  const int32 kCommandSize = 5;
  const size_t kNumArgs = kCommandSize - 1;
  COMPILE_ASSERT(kUsableNumCommandEntries % kCommandSize == 0,
                 Not_multiple_of_num_command_entries);
  CommandBufferEntry args1[kNumArgs];
  for (size_t ii = 0; ii < kNumArgs; ++ii) {
    args1[0].value_uint32 = ii + 1;
  }

  for (unsigned int i = 0; i < 5; ++i) {
    AddCommandWithExpect(
        error::kNoError, i + kUnusedCommandId, kNumArgs, args1);
  }

  helper_->Finish();
  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks that asking for available entries work, and that the parser
// effectively won't use that space.
TEST_F(CommandBufferHelperTest, TestAvailableEntries) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add 2 commands through the helper - 8 entries
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 1, 0, NULL);
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 2, 0, NULL);
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 3, 2, args);
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 4, 2, args);

  // Ask for 5 entries.
  helper_->WaitForAvailableEntries(5);

  CommandBufferOffset put = get_helper_put();
  CheckFreeSpace(put, 5);

  // Add more commands.
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 5, 2, args);

  // Wait until everything is done done.
  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

// Checks that the InsertToken/WaitForToken work.
TEST_F(CommandBufferHelperTest, TestToken) {
  CommandBufferEntry args[2];
  args[0].value_uint32 = 3;
  args[1].value_float = 4.f;

  // Add a first command.
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 3, 2, args);
  // keep track of the buffer position.
  CommandBufferOffset command1_put = get_helper_put();
  int32 token = helper_->InsertToken();

  EXPECT_CALL(*api_mock_.get(), DoCommand(cmd::kSetToken, 1, _))
      .WillOnce(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                      Return(error::kNoError)));
  // Add another command.
  AddCommandWithExpect(error::kNoError, kUnusedCommandId + 4, 2, args);
  helper_->WaitForToken(token);
  // check that the get pointer is beyond the first command.
  EXPECT_LE(command1_put, GetGetOffset());
  helper_->Finish();

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());

  // Check the error status.
  EXPECT_EQ(error::kNoError, GetError());
}

TEST_F(CommandBufferHelperTest, FreeRingBuffer) {
  EXPECT_TRUE(helper_->HaveRingBuffer());

  // Test freeing ring buffer.
  helper_->FreeRingBuffer();
  EXPECT_FALSE(helper_->HaveRingBuffer());

  // Test that InsertToken allocates a new one
  int32 token = helper_->InsertToken();
  EXPECT_TRUE(helper_->HaveRingBuffer());
  EXPECT_CALL(*api_mock_.get(), DoCommand(cmd::kSetToken, 1, _))
      .WillOnce(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                      Return(error::kNoError)));
  helper_->WaitForToken(token);
  helper_->FreeRingBuffer();
  EXPECT_FALSE(helper_->HaveRingBuffer());

  // Test that WaitForAvailableEntries allocates a new one
  AddCommandWithExpect(error::kNoError, kUnusedCommandId, 0, NULL);
  EXPECT_TRUE(helper_->HaveRingBuffer());
  helper_->Finish();
  helper_->FreeRingBuffer();
  EXPECT_FALSE(helper_->HaveRingBuffer());

  // Check that the commands did happen.
  Mock::VerifyAndClearExpectations(api_mock_.get());
}

TEST_F(CommandBufferHelperTest, Noop) {
  for (int ii = 1; ii < 4; ++ii) {
    CommandBufferOffset put_before = get_helper_put();
    helper_->Noop(ii);
    CommandBufferOffset put_after = get_helper_put();
    EXPECT_EQ(ii, put_after - put_before);
  }
}

}  // namespace gpu
