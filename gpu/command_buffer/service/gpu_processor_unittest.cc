// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/message_loop.h"
#include "gpu/command_buffer/common/command_buffer_mock.h"
#include "gpu/command_buffer/service/mocks.h"
#include "gpu/command_buffer/service/gpu_processor.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::NiceMock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace command_buffer {

const size_t kRingBufferSize = 1024;
const size_t kRingBufferEntries = kRingBufferSize / sizeof(int32);

class GPUProcessorTest : public testing::Test {
 protected:
  virtual void SetUp() {
    shared_memory_.reset(new ::base::SharedMemory);
    shared_memory_->Create(std::wstring(), false, false, kRingBufferSize);
    shared_memory_->Map(kRingBufferSize);
    buffer_ = static_cast<int32*>(shared_memory_->memory());

    memset(buffer_, 0, kRingBufferSize);

    command_buffer_.reset(new MockCommandBuffer);
    ON_CALL(*command_buffer_.get(), GetRingBuffer())
      .WillByDefault(Return(shared_memory_.get()));
    ON_CALL(*command_buffer_.get(), GetSize())
      .WillByDefault(Return(kRingBufferEntries));

    async_api_.reset(new StrictMock<command_buffer::AsyncAPIMock>);

    decoder_ = gles2::GLES2Decoder::Create();

    parser_ = new command_buffer::CommandParser(buffer_,
                                                kRingBufferEntries,
                                                0,
                                                kRingBufferEntries,
                                                0,
                                                async_api_.get());

    processor_ = new GPUProcessor(command_buffer_.get(),
                                  decoder_,
                                  parser_,
                                  2);
  }

  virtual void TearDown() {
    // Ensure that any unexpected tasks posted by the GPU processor are executed
    // in order to fail the test.
    MessageLoop::current()->RunAllPending();
  }

  base::AtExitManager at_exit_manager;
  MessageLoop message_loop;
  scoped_ptr<MockCommandBuffer> command_buffer_;
  scoped_ptr<::base::SharedMemory> shared_memory_;
  int32* buffer_;
  command_buffer::gles2::GLES2Decoder* decoder_;
  command_buffer::CommandParser* parser_;
  scoped_ptr<command_buffer::AsyncAPIMock> async_api_;
  scoped_refptr<GPUProcessor> processor_;
};

TEST_F(GPUProcessorTest, ProcessorDoesNothingIfRingBufferIsEmpty) {
  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(0));
  EXPECT_CALL(*command_buffer_, SetGetOffset(0));

  processor_->ProcessCommands();

  EXPECT_EQ(command_buffer::parse_error::kParseNoError,
            command_buffer_->ResetParseError());
  EXPECT_FALSE(command_buffer_->GetErrorStatus());
}

TEST_F(GPUProcessorTest, ProcessesOneCommand) {
  command_buffer::CommandHeader* header =
      reinterpret_cast<command_buffer::CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 2;
  buffer_[1] = 123;

  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(2));
  EXPECT_CALL(*command_buffer_, SetGetOffset(2));

  EXPECT_CALL(*async_api_, DoCommand(7, 1, &buffer_[0]))
    .WillOnce(Return(command_buffer::parse_error::kParseNoError));

  processor_->ProcessCommands();

  EXPECT_EQ(command_buffer::parse_error::kParseNoError,
            command_buffer_->ResetParseError());
  EXPECT_FALSE(command_buffer_->GetErrorStatus());
}

TEST_F(GPUProcessorTest, ProcessesTwoCommands) {
  command_buffer::CommandHeader* header =
      reinterpret_cast<command_buffer::CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 2;
  buffer_[1] = 123;
  header[2].command = 8;
  header[2].size = 1;

  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(3));
  EXPECT_CALL(*command_buffer_, SetGetOffset(3));

  EXPECT_CALL(*async_api_, DoCommand(7, 1, &buffer_[0]))
    .WillOnce(Return(command_buffer::parse_error::kParseNoError));

  EXPECT_CALL(*async_api_, DoCommand(8, 0, &buffer_[2]))
    .WillOnce(Return(command_buffer::parse_error::kParseNoError));

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, PostsTaskToFinishRemainingCommands) {
  command_buffer::CommandHeader* header =
      reinterpret_cast<command_buffer::CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 2;
  buffer_[1] = 123;
  header[2].command = 8;
  header[2].size = 1;
  header[3].command = 9;
  header[3].size = 1;

  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(4));

  EXPECT_CALL(*async_api_, DoCommand(7, 1, &buffer_[0]))
    .WillOnce(Return(command_buffer::parse_error::kParseNoError));

  EXPECT_CALL(*async_api_, DoCommand(8, 0, &buffer_[2]))
    .WillOnce(Return(command_buffer::parse_error::kParseNoError));

  EXPECT_CALL(*command_buffer_, SetGetOffset(3));

  processor_->ProcessCommands();

  // ProcessCommands is called a second time when the pending task is run.

  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(4));

  EXPECT_CALL(*async_api_, DoCommand(9, 0, &buffer_[3]))
    .WillOnce(Return(command_buffer::parse_error::kParseNoError));

  EXPECT_CALL(*command_buffer_, SetGetOffset(4));

  MessageLoop::current()->RunAllPending();
}

TEST_F(GPUProcessorTest, SetsErrorCodeOnCommandBuffer) {
  command_buffer::CommandHeader* header =
      reinterpret_cast<command_buffer::CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 1;

  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(1));
  EXPECT_CALL(*command_buffer_, SetGetOffset(1));

  EXPECT_CALL(*async_api_, DoCommand(7, 0, &buffer_[0]))
    .WillOnce(Return(
        command_buffer::parse_error::kParseUnknownCommand));

  EXPECT_CALL(*command_buffer_,
      SetParseError(command_buffer::parse_error::kParseUnknownCommand));

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest,
       RecoverableParseErrorsAreNotClearedByFollowingSuccessfulCommands) {
  command_buffer::CommandHeader* header =
      reinterpret_cast<command_buffer::CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 1;
  header[1].command = 8;
  header[1].size = 1;

  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(2));
  EXPECT_CALL(*command_buffer_, SetGetOffset(2));

  EXPECT_CALL(*async_api_, DoCommand(7, 0, &buffer_[0]))
    .WillOnce(Return(
        command_buffer::parse_error::kParseUnknownCommand));

  EXPECT_CALL(*async_api_, DoCommand(8, 0, &buffer_[1]))
    .WillOnce(Return(command_buffer::parse_error::kParseNoError));

  EXPECT_CALL(*command_buffer_,
      SetParseError(command_buffer::parse_error::kParseUnknownCommand));

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, UnrecoverableParseErrorsRaiseTheErrorStatus) {
  command_buffer::CommandHeader* header =
      reinterpret_cast<command_buffer::CommandHeader*>(&buffer_[0]);
  header[0].command = 7;
  header[0].size = 1;
  header[1].command = 8;
  header[1].size = 1;

  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .WillOnce(Return(2));

  EXPECT_CALL(*async_api_, DoCommand(7, 0, &buffer_[0]))
    .WillOnce(Return(command_buffer::parse_error::kParseInvalidSize));

  EXPECT_CALL(*command_buffer_,
      SetParseError(command_buffer::parse_error::kParseInvalidSize));

  EXPECT_CALL(*command_buffer_, RaiseErrorStatus());

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, ProcessCommandsDoesNothingAfterUnrecoverableError) {
  EXPECT_CALL(*command_buffer_, GetErrorStatus())
    .WillOnce(Return(true));

  EXPECT_CALL(*command_buffer_, GetPutOffset())
    .Times(0);

  processor_->ProcessCommands();
}

TEST_F(GPUProcessorTest, CanGetAddressOfSharedMemory) {
  EXPECT_CALL(*command_buffer_.get(), GetTransferBuffer(7))
    .WillOnce(Return(shared_memory_.get()));

  EXPECT_EQ(&buffer_[0], processor_->GetSharedMemoryAddress(7));
}

ACTION_P2(SetPointee, address, value) {
  *address = value;
}

TEST_F(GPUProcessorTest, GetAddressOfSharedMemoryMapsMemoryIfUnmapped) {
  EXPECT_CALL(*command_buffer_.get(), GetTransferBuffer(7))
    .WillOnce(Return(shared_memory_.get()));

  EXPECT_EQ(&buffer_[0], processor_->GetSharedMemoryAddress(7));
}

TEST_F(GPUProcessorTest, CanGetSizeOfSharedMemory) {
  EXPECT_CALL(*command_buffer_.get(), GetTransferBuffer(7))
    .WillOnce(Return(shared_memory_.get()));

  EXPECT_EQ(kRingBufferSize, processor_->GetSharedMemorySize(7));
}

TEST_F(GPUProcessorTest, SetTokenForwardsToCommandBuffer) {
  EXPECT_CALL(*command_buffer_, SetToken(7));
  processor_->set_token(7);
}

}  // namespace command_buffer
