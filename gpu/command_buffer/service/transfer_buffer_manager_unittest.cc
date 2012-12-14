// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/transfer_buffer_manager.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::SharedMemory;

namespace gpu {

class TransferBufferManagerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    TransferBufferManager* manager = new TransferBufferManager();
    transfer_buffer_manager_.reset(manager);
    manager->Initialize();
  }

  scoped_ptr<TransferBufferManagerInterface> transfer_buffer_manager_;
};

TEST_F(TransferBufferManagerTest, ZeroHandleMapsToNull) {
  EXPECT_TRUE(NULL == transfer_buffer_manager_->GetTransferBuffer(0).ptr);
}

TEST_F(TransferBufferManagerTest, NegativeHandleMapsToNull) {
  EXPECT_TRUE(NULL == transfer_buffer_manager_->GetTransferBuffer(-1).ptr);
}

TEST_F(TransferBufferManagerTest, OutOfRangeHandleMapsToNull) {
  EXPECT_TRUE(NULL == transfer_buffer_manager_->GetTransferBuffer(1).ptr);
}

TEST_F(TransferBufferManagerTest, CanCreateTransferBuffers) {
  int32 handle = transfer_buffer_manager_->CreateTransferBuffer(1024, -1);
  EXPECT_EQ(1, handle);
  Buffer buffer = transfer_buffer_manager_->GetTransferBuffer(handle);
  ASSERT_TRUE(NULL != buffer.ptr);
  EXPECT_EQ(1024u, buffer.size);
}

TEST_F(TransferBufferManagerTest, CreateTransferBufferReturnsDistinctHandles) {
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
}

TEST_F(TransferBufferManagerTest,
    CreateTransferBufferReusesUnregisteredHandles) {
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
  EXPECT_EQ(2, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
  EXPECT_EQ(3, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
}

TEST_F(TransferBufferManagerTest, CannotUnregisterHandleZero) {
  transfer_buffer_manager_->DestroyTransferBuffer(0);
  EXPECT_TRUE(NULL == transfer_buffer_manager_->GetTransferBuffer(0).ptr);
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
}

TEST_F(TransferBufferManagerTest, CannotUnregisterNegativeHandles) {
  transfer_buffer_manager_->DestroyTransferBuffer(-1);
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
}

TEST_F(TransferBufferManagerTest, CannotUnregisterUnregisteredHandles) {
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(TransferBufferManagerTest, UnregistersLastRegisteredHandle) {
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
}

// Testing this case specifically because there is an optimization that takes
// a different code path in this case.
TEST_F(TransferBufferManagerTest, UnregistersTwoLastRegisteredHandles) {
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
  EXPECT_EQ(2, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
  transfer_buffer_manager_->DestroyTransferBuffer(2);
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  EXPECT_EQ(1, transfer_buffer_manager_->CreateTransferBuffer(1024, -1));
}

}  // namespace gpu
