// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/thread.h"
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

  scoped_ptr<CommandBufferService> command_buffer_;
};

TEST_F(CommandBufferServiceTest, NullRingBufferByDefault) {
  EXPECT_TRUE(NULL == command_buffer_->GetRingBuffer());
}

TEST_F(CommandBufferServiceTest, InitializesCommandBuffer) {
  base::SharedMemory* ring_buffer = command_buffer_->Initialize(1024);
  EXPECT_TRUE(NULL != ring_buffer);
  EXPECT_EQ(ring_buffer, command_buffer_->GetRingBuffer());
  EXPECT_GT(command_buffer_->GetSize(), 0);
}

TEST_F(CommandBufferServiceTest, InitializeFailsSecondTime) {
  SharedMemory* ring_buffer = new SharedMemory;
  EXPECT_TRUE(NULL != command_buffer_->Initialize(1024));
  EXPECT_TRUE(NULL == command_buffer_->Initialize(1024));
}

TEST_F(CommandBufferServiceTest, GetAndPutOffsetsDefaultToZero) {
  EXPECT_EQ(0, command_buffer_->GetGetOffset());
  EXPECT_EQ(0, command_buffer_->GetPutOffset());
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
  EXPECT_EQ(0, command_buffer_->SyncOffsets(2));
  EXPECT_EQ(2, command_buffer_->GetPutOffset());

  EXPECT_CALL(*put_offset_change_callback, RunWithParams(_));
  EXPECT_EQ(0, command_buffer_->SyncOffsets(4));
  EXPECT_EQ(4, command_buffer_->GetPutOffset());

  command_buffer_->SetGetOffset(2);
  EXPECT_EQ(2, command_buffer_->GetGetOffset());
  EXPECT_CALL(*put_offset_change_callback, RunWithParams(_));
  EXPECT_EQ(2, command_buffer_->SyncOffsets(6));

  EXPECT_EQ(-1, command_buffer_->SyncOffsets(-1));
  EXPECT_EQ(-1, command_buffer_->SyncOffsets(1024));
}

TEST_F(CommandBufferServiceTest, ZeroHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(0));
}

TEST_F(CommandBufferServiceTest, NegativeHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(-1));
}

TEST_F(CommandBufferServiceTest, OutOfRangeHandleMapsToNull) {
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(1));
}

TEST_F(CommandBufferServiceTest, CanCreateTransferBuffers) {
  int32 handle = command_buffer_->CreateTransferBuffer(1024);
  EXPECT_EQ(1, handle);
  SharedMemory* buffer = command_buffer_->GetTransferBuffer(handle);
  ASSERT_TRUE(NULL != buffer);
  EXPECT_EQ(1024, buffer->max_size());
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
  EXPECT_TRUE(NULL == command_buffer_->GetTransferBuffer(0));
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
  EXPECT_EQ(0, command_buffer_->GetToken());
}

TEST_F(CommandBufferServiceTest, CanSetToken) {
  command_buffer_->SetToken(7);
  EXPECT_EQ(7, command_buffer_->GetToken());
}

TEST_F(CommandBufferServiceTest, DefaultParseErrorIsNoError) {
  EXPECT_EQ(0, command_buffer_->ResetParseError());
}

TEST_F(CommandBufferServiceTest, CanSetAndResetParseError) {
  command_buffer_->SetParseError(1);
  EXPECT_EQ(1, command_buffer_->ResetParseError());
  EXPECT_EQ(0, command_buffer_->ResetParseError());
}

TEST_F(CommandBufferServiceTest, DefaultErrorStatusIsFalse) {
  EXPECT_FALSE(command_buffer_->GetErrorStatus());
}

TEST_F(CommandBufferServiceTest, CanRaiseErrorStatus) {
  command_buffer_->RaiseErrorStatus();
  EXPECT_TRUE(command_buffer_->GetErrorStatus());
}

}  // namespace gpu
