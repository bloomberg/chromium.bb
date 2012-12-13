// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/transfer_buffer_manager.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::SharedMemory;

namespace gpu {

const static size_t kBufferSize = 1024;

class TransferBufferManagerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    for (size_t i = 0; i < arraysize(buffers_); ++i) {
      buffers_[i].CreateAnonymous(kBufferSize);
      buffers_[i].Map(kBufferSize);
    }

    TransferBufferManager* manager = new TransferBufferManager();
    transfer_buffer_manager_.reset(manager);
    ASSERT_TRUE(manager->Initialize());
  }

  base::SharedMemory buffers_[3];
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

TEST_F(TransferBufferManagerTest, CanRegisterTransferBuffer) {
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(1,
                                                               &buffers_[0],
                                                               kBufferSize));
  Buffer registered = transfer_buffer_manager_->GetTransferBuffer(1);

  // Distinct memory range and shared memory handle from that originally
  // registered.
  EXPECT_NE(static_cast<void*>(NULL), registered.ptr);
  EXPECT_NE(buffers_[0].memory(), registered.ptr);
  EXPECT_EQ(kBufferSize, registered.size);
  EXPECT_NE(&buffers_[0], registered.shared_memory);

  // But maps to the same physical memory.
  *static_cast<int*>(registered.ptr) = 7;
  *static_cast<int*>(buffers_[0].memory()) = 8;
  EXPECT_EQ(8, *static_cast<int*>(registered.ptr));
}

TEST_F(TransferBufferManagerTest, CanDestroyTransferBuffer) {
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(1,
                                                               &buffers_[0],
                                                               kBufferSize));
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  Buffer registered = transfer_buffer_manager_->GetTransferBuffer(1);

  EXPECT_EQ(static_cast<void*>(NULL), registered.ptr);
  EXPECT_EQ(0U, registered.size);
  EXPECT_EQ(static_cast<base::SharedMemory*>(NULL), registered.shared_memory);
}

TEST_F(TransferBufferManagerTest, CannotRegregisterTransferBufferId) {
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(1,
                                                               &buffers_[0],
                                                               kBufferSize));
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(1,
                                                                &buffers_[0],
                                                                kBufferSize));
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(1,
                                                                &buffers_[1],
                                                                kBufferSize));
}

TEST_F(TransferBufferManagerTest, CanReuseTransferBufferIdAfterDestroying) {
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(1,
                                                               &buffers_[0],
                                                               kBufferSize));
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(1,
                                                               &buffers_[1],
                                                               kBufferSize));
}

TEST_F(TransferBufferManagerTest, DestroyUnusedTransferBufferIdDoesNotCrash) {
  transfer_buffer_manager_->DestroyTransferBuffer(1);
}

TEST_F(TransferBufferManagerTest, CannotRegisterNullTransferBuffer) {
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(0,
                                                                &buffers_[0],
                                                                kBufferSize));
}

TEST_F(TransferBufferManagerTest, CannotRegisterNegativeTransferBufferId) {
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(-1,
                                                                &buffers_[0],
                                                                kBufferSize));
}

}  // namespace gpu
