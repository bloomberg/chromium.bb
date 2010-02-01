// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread.h"
#include "gpu/command_buffer/common/cmd_buffer_common.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::SharedMemory;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace gpu {

class CommandBufferServiceTest : public testing::Test {
 protected:
  virtual void SetUp() {
    command_buffer_.reset(new CommandBufferService);
  }

  int32 GetGetOffset() {
    return command_buffer_->GetState().get_offset;
  }

  int32 GetPutOffset() {
    return command_buffer_->GetState().put_offset;
  }

  int32 GetToken() {
    return command_buffer_->GetState().token;
  }

  int32 GetError() {
    return command_buffer_->GetState().error;
  }

  scoped_ptr<CommandBufferService> command_buffer_;
};

TEST_F(CommandBufferServiceTest, NullRingBufferByDefault) {
  EXPECT_TRUE(NULL == command_buffer_->GetRingBuffer().ptr);
}

TEST_F(CommandBufferServiceTest, InitializesCommandBuffer) {
  EXPECT_TRUE(command_buffer_->Initialize(1024));
  EXPECT_TRUE(NULL != command_buffer_->GetRingBuffer().ptr);
  CommandBuffer::State state = command_buffer_->GetState();
  EXPECT_EQ(1024, state.size);
  EXPECT_EQ(0, state.get_offset);
  EXPECT_EQ(0, state.put_offset);
  EXPECT_EQ(0, state.token);
  EXPECT_EQ(error::kNoError, state.error);
}

TEST_F(CommandBufferServiceTest, InitializationSizeIsInEntriesNotBytes) {
  EXPECT_TRUE(command_buffer_->Initialize(1024));
  EXPECT_TRUE(NULL != command_buffer_->GetRingBuffer().ptr);
  EXPECT_GE(1024 * sizeof(CommandBufferEntry),
            command_buffer_->GetRingBuffer().size);
}

TEST_F(CommandBufferServiceTest, InitializeFailsSecondTime) {
  SharedMemory* ring_buffer = new SharedMemory;
  EXPECT_TRUE(command_buffer_->Initialize(1024));
  EXPECT_FALSE(command_buffer_->Initialize(1024));
}

class MockCallback : public CallbackRunner<Tuple0> {
 public:
  MOCK_METHOD1(RunWithParams, void(const Tuple0&));
};

TEST_F(CommandBufferServiceTest, CanSyncGetAndPutOffset) {
  command_buffer_->Initialize(1024);

  StrictMock<MockCallback>* put_offset_change_callback =
      new StrictMock<MockCallback>;
  command_buffer_->SetPutOffsetChangeCallback(put_offset_change_callback);

  EXPECT_CALL(*put_offset_change_callback, RunWithParams(_));
  EXPECT_EQ(0, command_buffer_->Flush(2).get_offset);
  EXPECT_EQ(2, GetPutOffset());

  EXPECT_CALL(*put_offset_change_callback, RunWithParams(_));
  EXPECT_EQ(0, command_buffer_->Flush(4).get_offset);
  EXPECT_EQ(4, GetPutOffset());

  command_buffer_->SetGetOffset(2);
  EXPECT_EQ(2, GetGetOffset());
  EXPECT_CALL(*put_offset_change_callback, RunWithParams(_));
  EXPECT_EQ(2, command_buffer_->Flush(6).get_offset);

  EXPECT_NE(error::kNoError, command_buffer_->Flush(-1).error);
  EXPECT_NE(error::kNoError,
      command_buffer_->Flush(1024).error);
}

TEST_F(CommandBufferServiceTest, ZeroHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(0).ptr);
}

TEST_F(CommandBufferServiceTest, NegativeHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(-1).ptr);
}

TEST_F(CommandBufferServiceTest, OutOfRangeHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(1).ptr);
}

TEST_F(CommandBufferServiceTest, CanCreateTransferBuffers) {
  int32 handle = command_buffer_->CreateTransferBuffer(1024);
  EXPECT_EQ(1, handle);
  Buffer buffer = command_buffer_->GetTransferBuffer(handle);
  ASSERT_TRUE(NULL != buffer.ptr);
  EXPECT_EQ(1024, buffer.size);
}

TEST_F(CommandBufferServiceTest, CreateTransferBufferReturnsDistinctHandles) {
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferServiceTest,
    CreateTransferBufferReusesUnregisteredHandles) {
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
  EXPECT_EQ(2, command_buffer_->CreateTransferBuffer(1024));
  command_buffer_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
  EXPECT_EQ(3, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferServiceTest, CannotUnregisterHandleZero) {
  command_buffer_->DestroyTransferBuffer(0);
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(0).ptr);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferServiceTest, CannotUnregisterNegativeHandles) {
  command_buffer_->DestroyTransferBuffer(-1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferServiceTest, CannotUnregisterUnregisteredHandles) {
  command_buffer_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(CommandBufferServiceTest, UnregistersLastRegisteredHandle) {
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
  command_buffer_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(CommandBufferServiceTest, UnregistersTwoLastRegisteredHandles) {
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
  EXPECT_EQ(2, command_buffer_->CreateTransferBuffer(1024));
  command_buffer_->DestroyTransferBuffer(2);
  command_buffer_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, command_buffer_->CreateTransferBuffer(1024));
}

TEST_F(CommandBufferServiceTest, DefaultTokenIsZero) {
  EXPECT_EQ(0, GetToken());
}

TEST_F(CommandBufferServiceTest, CanSetToken) {
  command_buffer_->SetToken(7);
  EXPECT_EQ(7, GetToken());
}

TEST_F(CommandBufferServiceTest, DefaultParseErrorIsNoError) {
  EXPECT_EQ(0, GetError());
}

TEST_F(CommandBufferServiceTest, CanSetParseError) {
  command_buffer_->SetParseError(error::kInvalidSize);
  EXPECT_EQ(1, GetError());
}
}  // namespace gpu
