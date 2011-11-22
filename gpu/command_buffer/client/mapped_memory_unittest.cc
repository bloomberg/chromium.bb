// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/mapped_memory.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
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

class MappedMemoryTestBase : public testing::Test {
 protected:
  static const unsigned int kBufferSize = 1024;

  virtual void SetUp() {
    api_mock_.reset(new AsyncAPIMock);
    // ignore noops in the mock - we don't want to inspect the internals of the
    // helper.
    EXPECT_CALL(*api_mock_, DoCommand(cmd::kNoop, 0, _))
        .WillRepeatedly(Return(error::kNoError));
    // Forward the SetToken calls to the engine
    EXPECT_CALL(*api_mock_.get(), DoCommand(cmd::kSetToken, 1, _))
        .WillRepeatedly(DoAll(Invoke(api_mock_.get(), &AsyncAPIMock::SetToken),
                              Return(error::kNoError)));

    command_buffer_.reset(new CommandBufferService);
    command_buffer_->Initialize(kBufferSize);
    Buffer ring_buffer = command_buffer_->GetRingBuffer();

    parser_ = new CommandParser(ring_buffer.ptr,
                                ring_buffer.size,
                                0,
                                ring_buffer.size,
                                0,
                                api_mock_.get());

    gpu_scheduler_.reset(new GpuScheduler(
        command_buffer_.get(), NULL, parser_));
    command_buffer_->SetPutOffsetChangeCallback(base::Bind(
        &GpuScheduler::PutChanged, base::Unretained(gpu_scheduler_.get())));

    api_mock_->set_engine(gpu_scheduler_.get());

    helper_.reset(new CommandBufferHelper(command_buffer_.get()));
    helper_->Initialize(kBufferSize);
  }

  int32 GetToken() {
    return command_buffer_->GetState().token;
  }

#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool_;
#endif
  MessageLoop message_loop_;
  scoped_ptr<AsyncAPIMock> api_mock_;
  scoped_ptr<CommandBufferService> command_buffer_;
  scoped_ptr<GpuScheduler> gpu_scheduler_;
  CommandParser* parser_;
  scoped_ptr<CommandBufferHelper> helper_;
};

#ifndef _MSC_VER
const unsigned int MappedMemoryTestBase::kBufferSize;
#endif

// Test fixture for MemoryChunk test - Creates a MemoryChunk, using a
// CommandBufferHelper with a mock AsyncAPIInterface for its interface (calling
// it directly, not through the RPC mechanism), making sure Noops are ignored
// and SetToken are properly forwarded to the engine.
class MemoryChunkTest : public MappedMemoryTestBase {
 protected:
  static const int32 kShmId = 123;
  virtual void SetUp() {
    MappedMemoryTestBase::SetUp();
    buffer_.reset(new uint8[kBufferSize]);
    gpu::Buffer buf;
    buf.size = kBufferSize;
    buf.ptr = buffer_.get();
    chunk_.reset(new MemoryChunk(kShmId, buf, helper_.get()));
  }

  virtual void TearDown() {
    // If the GpuScheduler posts any tasks, this forces them to run.
    MessageLoop::current()->RunAllPending();

    MappedMemoryTestBase::TearDown();
  }

  scoped_ptr<MemoryChunk> chunk_;
  scoped_array<uint8> buffer_;
};

#ifndef _MSC_VER
const int32 MemoryChunkTest::kShmId;
#endif

TEST_F(MemoryChunkTest, Basic) {
  const unsigned int kSize = 16;
  EXPECT_EQ(kShmId, chunk_->shm_id());
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetSize());
  void *pointer = chunk_->Alloc(kSize);
  ASSERT_TRUE(pointer);
  EXPECT_LE(buffer_.get(), static_cast<uint8 *>(pointer));
  EXPECT_GE(kBufferSize, static_cast<uint8 *>(pointer) - buffer_.get() + kSize);
  EXPECT_EQ(kBufferSize - kSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize - kSize, chunk_->GetLargestFreeSizeWithWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetSize());

  chunk_->Free(pointer);
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithWaiting());

  uint8 *pointer_char = static_cast<uint8*>(chunk_->Alloc(kSize));
  ASSERT_TRUE(pointer_char);
  EXPECT_LE(buffer_.get(), pointer_char);
  EXPECT_GE(buffer_.get() + kBufferSize, pointer_char + kSize);
  EXPECT_EQ(kBufferSize - kSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize - kSize, chunk_->GetLargestFreeSizeWithWaiting());
  chunk_->Free(pointer_char);
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithoutWaiting());
  EXPECT_EQ(kBufferSize, chunk_->GetLargestFreeSizeWithWaiting());
}

class MappedMemoryManagerTest : public MappedMemoryTestBase {
 protected:
  virtual void SetUp() {
    MappedMemoryTestBase::SetUp();
    manager_.reset(new MappedMemoryManager(helper_.get()));
  }

  virtual void TearDown() {
    // If the GpuScheduler posts any tasks, this forces them to run.
    MessageLoop::current()->RunAllPending();
    manager_.reset();
    MappedMemoryTestBase::TearDown();
  }

  scoped_ptr<MappedMemoryManager> manager_;
};

TEST_F(MappedMemoryManagerTest, Basic) {
  const unsigned int kSize = 1024;
  // Check we can alloc.
  int32 id1 = -1;
  unsigned int offset1 = 0xFFFFFFFFU;
  void* mem1 = manager_->Alloc(kSize, &id1, &offset1);
  ASSERT_TRUE(mem1);
  EXPECT_NE(-1, id1);
  EXPECT_EQ(0u, offset1);
  // Check if we free and realloc the same size we get the same memory
  int32 id2 = -1;
  unsigned int offset2 = 0xFFFFFFFFU;
  manager_->Free(mem1);
  void* mem2 = manager_->Alloc(kSize, &id2, &offset2);
  EXPECT_EQ(mem1, mem2);
  EXPECT_EQ(id1, id2);
  EXPECT_EQ(offset1, offset2);
  // Check if we allocate again we get different shared memory
  int32 id3 = -1;
  unsigned int offset3 = 0xFFFFFFFFU;
  void* mem3 = manager_->Alloc(kSize, &id3, &offset3);
  ASSERT_TRUE(mem3 != NULL);
  EXPECT_NE(mem2, mem3);
  EXPECT_NE(id2, id3);
  EXPECT_EQ(0u, offset3);
  // Free 3 and allocate 2 half size blocks.
  manager_->Free(mem3);
  int32 id4 = -1;
  int32 id5 = -1;
  unsigned int offset4 = 0xFFFFFFFFU;
  unsigned int offset5 = 0xFFFFFFFFU;
  void* mem4 = manager_->Alloc(kSize / 2, &id4, &offset4);
  void* mem5 = manager_->Alloc(kSize / 2, &id5, &offset5);
  ASSERT_TRUE(mem4 != NULL);
  ASSERT_TRUE(mem5 != NULL);
  EXPECT_EQ(id3, id4);
  EXPECT_EQ(id4, id5);
  EXPECT_EQ(0u, offset4);
  EXPECT_EQ(kSize / 2u, offset5);
  manager_->Free(mem4);
  manager_->Free(mem2);
  manager_->Free(mem5);
}

TEST_F(MappedMemoryManagerTest, FreePendingToken) {
  const unsigned int kSize = 128;
  const unsigned int kAllocCount = (kBufferSize / kSize) * 2;
  CHECK(kAllocCount * kSize == kBufferSize * 2);

  // Allocate several buffers across multiple chunks.
  void *pointers[kAllocCount];
  for (unsigned int i = 0; i < kAllocCount; ++i) {
    int32 id = -1;
    unsigned int offset = 0xFFFFFFFFu;
    pointers[i] = manager_->Alloc(kSize, &id, &offset);
    EXPECT_TRUE(pointers[i]);
    EXPECT_NE(id, -1);
    EXPECT_NE(offset, 0xFFFFFFFFu);
  }

  // Free one successful allocation, pending fence.
  int32 token = helper_.get()->InsertToken();
  manager_->FreePendingToken(pointers[0], token);

  // The way we hooked up the helper and engine, it won't process commands
  // until it has to wait for something. Which means the token shouldn't have
  // passed yet at this point.
  EXPECT_GT(token, GetToken());
  // Force it to read up to the token
  helper_->Finish();
  // Check that the token has indeed passed.
  EXPECT_LE(token, GetToken());

  // This allocation should use the spot just freed above.
  int32 new_id = -1;
  unsigned int new_offset = 0xFFFFFFFFu;
  void* new_ptr = manager_->Alloc(kSize, &new_id, &new_offset);
  EXPECT_TRUE(new_ptr);
  EXPECT_EQ(new_ptr, pointers[0]);
  EXPECT_NE(new_id, -1);
  EXPECT_NE(new_offset, 0xFFFFFFFFu);

  // Free up everything.
  manager_->Free(new_ptr);
  for (unsigned int i = 1; i < kAllocCount; ++i) {
    manager_->Free(pointers[i]);
  }
}

// Check if we don't free we don't crash.
TEST_F(MappedMemoryManagerTest, DontFree) {
  const unsigned int kSize = 1024;
  // Check we can alloc.
  int32 id1 = -1;
  unsigned int offset1 = 0xFFFFFFFFU;
  void* mem1 = manager_->Alloc(kSize, &id1, &offset1);
  ASSERT_TRUE(mem1);
}

TEST_F(MappedMemoryManagerTest, FreeUnused) {
  int32 id = -1;
  unsigned int offset = 0xFFFFFFFFU;
  void* m1 = manager_->Alloc(kBufferSize, &id, &offset);
  void* m2 = manager_->Alloc(kBufferSize, &id, &offset);
  ASSERT_TRUE(m1 != NULL);
  ASSERT_TRUE(m2 != NULL);
  EXPECT_EQ(2u, manager_->num_chunks());
  manager_->FreeUnused();
  EXPECT_EQ(2u, manager_->num_chunks());
  manager_->Free(m2);
  EXPECT_EQ(2u, manager_->num_chunks());
  manager_->FreeUnused();
  EXPECT_EQ(1u, manager_->num_chunks());
  manager_->Free(m1);
  EXPECT_EQ(1u, manager_->num_chunks());
  manager_->FreeUnused();
  EXPECT_EQ(0u, manager_->num_chunks());
}

TEST_F(MappedMemoryManagerTest, ChunkSizeMultiple) {
  const unsigned int kSize = 1024;
  manager_->set_chunk_size_multiple(kSize *  2);
  // Check if we allocate less than the chunk size multiple we get
  // chunks arounded up.
  int32 id1 = -1;
  unsigned int offset1 = 0xFFFFFFFFU;
  void* mem1 = manager_->Alloc(kSize, &id1, &offset1);
  int32 id2 = -1;
  unsigned int offset2 = 0xFFFFFFFFU;
  void* mem2 = manager_->Alloc(kSize, &id2, &offset2);
  int32 id3 = -1;
  unsigned int offset3 = 0xFFFFFFFFU;
  void* mem3 = manager_->Alloc(kSize, &id3, &offset3);
  ASSERT_TRUE(mem1);
  ASSERT_TRUE(mem2);
  ASSERT_TRUE(mem3);
  EXPECT_NE(-1, id1);
  EXPECT_EQ(id1, id2);
  EXPECT_NE(id2, id3);
  EXPECT_EQ(0u, offset1);
  EXPECT_EQ(kSize, offset2);
  EXPECT_EQ(0u, offset3);
}

}  // namespace gpu
