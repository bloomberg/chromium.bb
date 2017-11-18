// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/threading/thread_task_runner_handle.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "cc/paint/transfer_cache_entry.h"
#include "cc/test/test_in_process_context_provider.h"
#include "gpu/command_buffer/client/client_transfer_cache.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/service/service_transfer_cache.h"
#include "gpu/ipc/gl_in_process_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_implementation.h"

namespace cc {
namespace {

class TransferCacheTest : public testing::Test {
 public:
  TransferCacheTest()
      : testing::Test(), test_client_entry_(std::vector<uint8_t>(100)) {}
  void SetUp() override {
    bool is_offscreen = true;
    gpu::gles2::ContextCreationAttribHelper attribs;
    attribs.alpha_size = -1;
    attribs.depth_size = 24;
    attribs.stencil_size = 8;
    attribs.samples = 0;
    attribs.sample_buffers = 0;
    attribs.fail_if_major_perf_caveat = false;
    attribs.bind_generates_resource = false;

    context_ = gpu::GLInProcessContext::CreateWithoutInit();
    auto result = context_->Initialize(
        nullptr, nullptr, is_offscreen, gpu::kNullSurfaceHandle, nullptr,
        attribs, gpu::SharedMemoryLimits(), &gpu_memory_buffer_manager_,
        &image_factory_, base::ThreadTaskRunnerHandle::Get());

    ASSERT_EQ(result, gpu::ContextResult::kSuccess);
  }

  void TearDown() override { context_.reset(); }

  gpu::ClientTransferCache* ClientTransferCache() {
    return &client_transfer_cache_;
  }

  gpu::ServiceTransferCache* ServiceTransferCache() {
    return context_->ContextGroupForTesting()->transfer_cache();
  }

  gpu::gles2::GLES2Implementation* Gl() {
    return context_->GetImplementation();
  }

  gpu::CommandBuffer* CommandBuffer() {
    return context_->GetImplementation()->helper()->command_buffer();
  }

  const ClientRawMemoryTransferCacheEntry& test_client_entry() const {
    return test_client_entry_;
  }

 private:
  gpu::ClientTransferCache client_transfer_cache_;
  viz::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  TestImageFactory image_factory_;
  std::unique_ptr<gpu::GLInProcessContext> context_;
  gl::DisableNullDrawGLBindings enable_pixel_output_;
  ClientRawMemoryTransferCacheEntry test_client_entry_;
};

TEST_F(TransferCacheTest, Basic) {
  auto* client_cache = ClientTransferCache();
  auto* service_cache = ServiceTransferCache();
  auto* gl = Gl();
  auto* command_buffer = CommandBuffer();

  // Create an entry and validate client-side state.
  gpu::TransferCacheEntryId id =
      client_cache->CreateCacheEntry(gl, command_buffer, test_client_entry());
  EXPECT_FALSE(id.is_null());
  gpu::ClientDiscardableHandle handle =
      client_cache->DiscardableManagerForTesting()->GetHandle(id);
  EXPECT_TRUE(handle.IsLockedForTesting());
  gl->Finish();

  // Validate service-side state.
  EXPECT_NE(nullptr, service_cache->GetEntry(id));

  // Unlock on client side and flush to service. Validate handle state.
  client_cache->UnlockTransferCacheEntry(gl, id);
  gl->Finish();
  EXPECT_FALSE(handle.IsLockedForTesting());

  // Re-lock on client side and validate state. Nop need to flush as lock is
  // local.
  EXPECT_TRUE(client_cache->LockTransferCacheEntry(id));
  EXPECT_TRUE(handle.IsLockedForTesting());

  // Delete on client side, flush, and validate that deletion reaches service.
  client_cache->DeleteTransferCacheEntry(gl, id);
  gl->Finish();
  EXPECT_TRUE(handle.IsDeletedForTesting());
  EXPECT_EQ(nullptr, service_cache->GetEntry(id));
}

TEST_F(TransferCacheTest, Eviction) {
  auto* client_cache = ClientTransferCache();
  auto* service_cache = ServiceTransferCache();
  auto* gl = Gl();
  auto* command_buffer = CommandBuffer();

  // Create an entry and validate client-side state.
  gpu::TransferCacheEntryId id =
      client_cache->CreateCacheEntry(gl, command_buffer, test_client_entry());
  EXPECT_FALSE(id.is_null());
  gpu::ClientDiscardableHandle handle =
      client_cache->DiscardableManagerForTesting()->GetHandle(id);
  EXPECT_TRUE(handle.IsLockedForTesting());
  gl->Finish();

  // Validate service-side state.
  EXPECT_NE(nullptr, service_cache->GetEntry(id));

  // Unlock on client side and flush to service. Validate handle state.
  client_cache->UnlockTransferCacheEntry(gl, id);
  gl->Finish();
  EXPECT_FALSE(handle.IsLockedForTesting());

  // Evict on the service side.
  service_cache->SetCacheSizeLimitForTesting(0);
  EXPECT_TRUE(handle.IsDeletedForTesting());
  EXPECT_EQ(nullptr, service_cache->GetEntry(id));

  // Try to re-lock on the client side. This should fail.
  EXPECT_FALSE(client_cache->LockTransferCacheEntry(id));
  EXPECT_FALSE(handle.IsLockedForTesting());
}

TEST_F(TransferCacheTest, RawMemoryTransfer) {
  auto* client_cache = ClientTransferCache();
  auto* service_cache = ServiceTransferCache();
  auto* gl = Gl();
  auto* command_buffer = CommandBuffer();

  // Create an entry with some initialized data.
  std::vector<uint8_t> data;
  data.resize(100);
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = i;
  }

  // Add the entry to the transfer cache
  ClientRawMemoryTransferCacheEntry client_entry(data);
  gpu::TransferCacheEntryId id =
      client_cache->CreateCacheEntry(gl, command_buffer, client_entry);
  EXPECT_FALSE(id.is_null());
  gpu::ClientDiscardableHandle handle =
      client_cache->DiscardableManagerForTesting()->GetHandle(id);
  EXPECT_TRUE(handle.IsLockedForTesting());
  gl->Finish();

  // Validate service-side data matches.
  ServiceTransferCacheEntry* service_entry = service_cache->GetEntry(id);
  EXPECT_EQ(service_entry->Type(), client_entry.Type());
  const std::vector<uint8_t> service_data =
      static_cast<ServiceRawMemoryTransferCacheEntry*>(service_entry)->data();
  EXPECT_EQ(data, service_data);
}

// A TransferCacheEntry that intentionally constructs on invalid
// TransferCacheEntryType.
class InvalidIdTransferCacheEntry : public ClientTransferCacheEntry {
 public:
  ~InvalidIdTransferCacheEntry() override = default;
  TransferCacheEntryType Type() const override {
    return static_cast<TransferCacheEntryType>(
        static_cast<uint32_t>(TransferCacheEntryType::kLast) + 1);
  }
  size_t SerializedSize() const override { return sizeof(uint32_t); }
  bool Serialize(size_t size, uint8_t* data) const override { return true; }
};

TEST_F(TransferCacheTest, InvalidTypeFails) {
  auto* client_cache = ClientTransferCache();
  auto* service_cache = ServiceTransferCache();
  auto* gl = Gl();
  auto* command_buffer = CommandBuffer();

  // Add the entry to the transfer cache
  gpu::TransferCacheEntryId id = client_cache->CreateCacheEntry(
      gl, command_buffer, InvalidIdTransferCacheEntry());
  EXPECT_FALSE(id.is_null());
  gpu::ClientDiscardableHandle handle =
      client_cache->DiscardableManagerForTesting()->GetHandle(id);
  EXPECT_TRUE(handle.IsLockedForTesting());
  gl->Finish();

  // Nothing should be created service side, as the type was invalid.
  EXPECT_EQ(nullptr, service_cache->GetEntry(id));
}

}  // namespace
}  // namespace cc
