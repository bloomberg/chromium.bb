// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the command parser.

#include <stddef.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/client/client_test_helper.h"
#include "gpu/command_buffer/service/command_executor.h"
#include "gpu/command_buffer/service/mocks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

using testing::_;
using testing::Mock;
using testing::Return;
using testing::Sequence;
using testing::SetArgPointee;

namespace {

class FakeCommandBufferServiceBase : public MockCommandBufferBase {
 public:
  FakeCommandBufferServiceBase() {}
  ~FakeCommandBufferServiceBase() override {}

  void Flush(int32_t put_offset) override { FlushHelper(put_offset); }
  void OrderingBarrier(int32_t put_offset) override { FlushHelper(put_offset); }
  void DestroyTransferBuffer(int32_t id) override {
    DestroyTransferBufferHelper(id);
  }

  void OnFlush() override {}
};

}  // anonymous namespace

// Test fixture for CommandExecutor test - Creates a mock AsyncAPIInterface, and
// a fixed size memory buffer. Also provides a simple API to create a
// CommandExecutor.
class CommandExecutorTest : public testing::Test {
 protected:
  virtual void SetUp() { api_mock_.reset(new AsyncAPIMock(false)); }
  virtual void TearDown() {}

  void AddDoCommandsExpect(error::Error _return,
                           int num_entries,
                           int num_processed) {
    EXPECT_CALL(*api_mock_, DoCommands(_, _, num_entries, _))
        .InSequence(sequence_)
        .WillOnce(DoAll(SetArgPointee<3>(num_processed), Return(_return)));
  }

  // Creates an executor, with a buffer of the specified size (in entries).
  void MakeExecutor(unsigned int entry_count) {
    command_buffer_ = base::MakeUnique<FakeCommandBufferServiceBase>();
    executor_.reset(
        new CommandExecutor(command_buffer_.get(), api_mock(), nullptr));
    SetNewGetBuffer(entry_count * sizeof(CommandBufferEntry));
  }

  AsyncAPIMock* api_mock() { return api_mock_.get(); }
  CommandBufferEntry* buffer() {
    return static_cast<CommandBufferEntry*>(buffer_->memory());
  }

  FakeCommandBufferServiceBase* command_buffer() {
    return command_buffer_.get();
  }
  CommandExecutor* executor() { return executor_.get(); }

  int32_t GetGet() { return command_buffer_->GetLastState().get_offset; }
  int32_t GetPut() { return command_buffer_->GetPutOffset(); }

  error::Error SetPutAndProcessAllCommands(int32_t put) {
    command_buffer_->Flush(put);
    EXPECT_EQ(put, GetPut());
    executor_->PutChanged();
    return command_buffer_->GetLastState().error;
  }

  int32_t SetNewGetBuffer(size_t size) {
    int32_t id = 0;
    buffer_ = command_buffer_->CreateTransferBuffer(size, &id);
    command_buffer_->SetGetBuffer(id);
    executor_->SetGetBuffer(id);
    return id;
  }

 private:
  std::unique_ptr<AsyncAPIMock> api_mock_;
  std::unique_ptr<FakeCommandBufferServiceBase> command_buffer_;
  scoped_refptr<Buffer> buffer_;
  std::unique_ptr<CommandExecutor> executor_;
  Sequence sequence_;
};

// Tests initialization conditions.
TEST_F(CommandExecutorTest, TestInit) {
  MakeExecutor(10);
  EXPECT_EQ(0, GetGet());
  EXPECT_EQ(0, GetPut());
}

TEST_F(CommandExecutorTest, TestEmpty) {
  MakeExecutor(10);
  EXPECT_CALL(*api_mock(), DoCommands(_, _, _, _)).Times(0);

  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(0));
  EXPECT_EQ(0, GetGet());
}

// Tests simple commands.
TEST_F(CommandExecutorTest, TestSimple) {
  MakeExecutor(10);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // add a single command, no args
  header.size = 1;
  header.command = 123;
  buffer()[put++].value_header = header;

  AddDoCommandsExpect(error::kNoError, 1, 1);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());

  // add a single command, 2 args
  header.size = 3;
  header.command = 456;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 2134;
  buffer()[put++].value_float = 1.f;

  AddDoCommandsExpect(error::kNoError, 3, 3);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests having multiple commands in the buffer.
TEST_F(CommandExecutorTest, TestMultipleCommands) {
  MakeExecutor(10);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // add 2 commands, test with single ProcessAllCommands()
  header.size = 2;
  header.command = 789;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5151;

  header.size = 2;
  header.command = 876;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 3434;

  // Process commands.  4 entries remaining.
  AddDoCommandsExpect(error::kNoError, 4, 4);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 2 commands again, test with ProcessAllCommands()
  header.size = 2;
  header.command = 123;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5656;

  header.size = 2;
  header.command = 321;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 7878;

  // 4 entries remaining.
  AddDoCommandsExpect(error::kNoError, 4, 4);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests that the parser will wrap correctly at the end of the buffer.
TEST_F(CommandExecutorTest, TestWrap) {
  MakeExecutor(5);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // add 3 commands with no args (1 word each)
  for (unsigned int i = 0; i < 3; ++i) {
    header.size = 1;
    header.command = i;
    buffer()[put++].value_header = header;
  }

  // Process up to 10 commands.  3 entries remaining to put.
  AddDoCommandsExpect(error::kNoError, 3, 3);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());

  // add 1 command with 1 arg (2 words). That should put us at the end of the
  // buffer.
  header.size = 2;
  header.command = 3;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 5;

  DCHECK_EQ(5, put);
  put = 0;

  // add 1 command with 1 arg (2 words).
  header.size = 2;
  header.command = 4;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 6;

  // 2 entries remaining to end of buffer.
  AddDoCommandsExpect(error::kNoError, 2, 2);
  // 2 entries remaining to put.
  AddDoCommandsExpect(error::kNoError, 2, 2);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
}

// Tests error conditions.
TEST_F(CommandExecutorTest, TestError) {
  const unsigned int kNumEntries = 5;
  MakeExecutor(kNumEntries);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // Generate a command with size 0.
  header.size = 0;
  header.command = 3;
  buffer()[put++].value_header = header;

  AddDoCommandsExpect(error::kInvalidSize, 1, 0);
  EXPECT_EQ(error::kInvalidSize, SetPutAndProcessAllCommands(put));
  // check that no DoCommand call was made.
  Mock::VerifyAndClearExpectations(api_mock());

  MakeExecutor(5);
  put = GetPut();

  // Generate a command with size 6, extends beyond the end of the buffer.
  header.size = 6;
  header.command = 3;
  buffer()[put++].value_header = header;

  AddDoCommandsExpect(error::kOutOfBounds, 1, 0);
  EXPECT_EQ(error::kOutOfBounds, SetPutAndProcessAllCommands(put));
  // check that no DoCommand call was made.
  Mock::VerifyAndClearExpectations(api_mock());

  MakeExecutor(5);
  put = GetPut();

  // Generates 2 commands.
  header.size = 1;
  header.command = 3;
  buffer()[put++].value_header = header;
  CommandBufferOffset put_post_fail = put;
  header.size = 1;
  header.command = 4;
  buffer()[put++].value_header = header;

  // have the first command fail to parse.
  AddDoCommandsExpect(error::kUnknownCommand, 2, 1);
  EXPECT_EQ(error::kUnknownCommand, SetPutAndProcessAllCommands(put));
  // check that only one command was executed, and that get reflects that
  // correctly.
  EXPECT_EQ(put_post_fail, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
  // make the second one succeed, and check that the executor doesn't try to
  // recover.
  EXPECT_EQ(error::kUnknownCommand, SetPutAndProcessAllCommands(put));
  EXPECT_EQ(put_post_fail, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());
}

TEST_F(CommandExecutorTest, SetBuffer) {
  MakeExecutor(3);
  CommandBufferOffset put = GetPut();
  CommandHeader header;

  // add a single command, no args
  header.size = 2;
  header.command = 123;
  buffer()[put++].value_header = header;
  buffer()[put++].value_int32 = 456;

  AddDoCommandsExpect(error::kNoError, 2, 2);
  EXPECT_EQ(error::kNoError, SetPutAndProcessAllCommands(put));
  // We should have advanced 2 entries
  EXPECT_EQ(2, GetGet());
  Mock::VerifyAndClearExpectations(api_mock());

  SetNewGetBuffer(2 * sizeof(CommandBufferEntry));
  // The put and get should have reset to 0.
  EXPECT_EQ(0, GetGet());
  EXPECT_EQ(0, GetPut());
}

TEST_F(CommandExecutorTest, CanGetSharedMemory) {
  MakeExecutor(3);
  int32_t id = 0;
  size_t size = 0x1234;
  scoped_refptr<Buffer> buffer =
      command_buffer()->CreateTransferBuffer(size, &id);

  EXPECT_EQ(buffer, executor()->GetSharedMemoryBuffer(id));
}

TEST_F(CommandExecutorTest, SetTokenForwardsToCommandBuffer) {
  MakeExecutor(3);
  executor()->set_token(7);
  EXPECT_EQ(7, command_buffer()->GetLastState().token);
}

}  // namespace gpu
