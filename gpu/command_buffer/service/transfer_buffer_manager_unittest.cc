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
    TransferBufferManager* manager = new TransferBufferManager();
    transfer_buffer_manager_.reset(manager);
    ASSERT_TRUE(manager->Initialize());
  }

  scoped_ptr<TransferBufferManagerInterface> transfer_buffer_manager_;
};

TEST_F(TransferBufferManagerTest, ZeroHandleMapsToNull) {
  EXPECT_TRUE(NULL == transfer_buffer_manager_->GetTransferBuffer(0));
}

TEST_F(TransferBufferManagerTest, NegativeHandleMapsToNull) {
  EXPECT_TRUE(NULL == transfer_buffer_manager_->GetTransferBuffer(-1));
}

TEST_F(TransferBufferManagerTest, OutOfRangeHandleMapsToNull) {
  EXPECT_TRUE(NULL == transfer_buffer_manager_->GetTransferBuffer(1));
}

TEST_F(TransferBufferManagerTest, CanRegisterTransferBuffer) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  shm->CreateAndMapAnonymous(kBufferSize);
  base::SharedMemory* shm_raw_pointer = shm.get();
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, shm.Pass(), kBufferSize));
  scoped_refptr<Buffer> registered =
      transfer_buffer_manager_->GetTransferBuffer(1);

  // Shared-memory ownership is transfered. It should be the same memory.
  EXPECT_EQ(shm_raw_pointer, registered->shared_memory());
}

TEST_F(TransferBufferManagerTest, CanDestroyTransferBuffer) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  shm->CreateAndMapAnonymous(kBufferSize);
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, shm.Pass(), kBufferSize));
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  scoped_refptr<Buffer> registered =
      transfer_buffer_manager_->GetTransferBuffer(1);

  scoped_refptr<Buffer> null_buffer;
  EXPECT_EQ(null_buffer, registered);
}

TEST_F(TransferBufferManagerTest, CannotRegregisterTransferBufferId) {
  scoped_ptr<base::SharedMemory> shm1(new base::SharedMemory());
  scoped_ptr<base::SharedMemory> shm2(new base::SharedMemory());
  scoped_ptr<base::SharedMemory> shm3(new base::SharedMemory());
  shm1->CreateAndMapAnonymous(kBufferSize);
  shm2->CreateAndMapAnonymous(kBufferSize);
  shm3->CreateAndMapAnonymous(kBufferSize);

  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, shm1.Pass(), kBufferSize));
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, shm2.Pass(), kBufferSize));
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, shm3.Pass(), kBufferSize));
}

TEST_F(TransferBufferManagerTest, CanReuseTransferBufferIdAfterDestroying) {
  scoped_ptr<base::SharedMemory> shm1(new base::SharedMemory());
  scoped_ptr<base::SharedMemory> shm2(new base::SharedMemory());
  shm1->CreateAndMapAnonymous(kBufferSize);
  shm2->CreateAndMapAnonymous(kBufferSize);
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, shm1.Pass(), kBufferSize));
  transfer_buffer_manager_->DestroyTransferBuffer(1);
  EXPECT_TRUE(transfer_buffer_manager_->RegisterTransferBuffer(
      1, shm2.Pass(), kBufferSize));
}

TEST_F(TransferBufferManagerTest, DestroyUnusedTransferBufferIdDoesNotCrash) {
  transfer_buffer_manager_->DestroyTransferBuffer(1);
}

TEST_F(TransferBufferManagerTest, CannotRegisterNullTransferBuffer) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  shm->CreateAndMapAnonymous(kBufferSize);
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(
      0, shm.Pass(), kBufferSize));
}

TEST_F(TransferBufferManagerTest, CannotRegisterNegativeTransferBufferId) {
  scoped_ptr<base::SharedMemory> shm(new base::SharedMemory());
  shm->CreateAndMapAnonymous(kBufferSize);
  EXPECT_FALSE(transfer_buffer_manager_->RegisterTransferBuffer(
      -1, shm.Pass(), kBufferSize));
}

}  // namespace gpu
