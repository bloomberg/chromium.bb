// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/server_gpu_memory_buffer_manager.h"

#include "base/test/scoped_task_environment.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "services/ui/gpu/interfaces/gpu_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/client_native_pixmap_factory.h"

namespace viz {

namespace {

class TestGpuService : public ui::mojom::GpuService {
 public:
  TestGpuService() {}
  ~TestGpuService() override {}

  bool HasAllocationRequest(gfx::GpuMemoryBufferId id, int client_id) const {
    for (const auto& req : allocation_requests_) {
      if (req.id == id && req.client_id == client_id)
        return true;
    }
    return false;
  }

  bool HasDestructionRequest(gfx::GpuMemoryBufferId id, int client_id) const {
    for (const auto& req : destruction_requests_) {
      if (req.id == id && req.client_id == client_id)
        return true;
    }
    return false;
  }

  void SatisfyAllocationRequest(gfx::GpuMemoryBufferId id, int client_id) {
    for (const auto& req : allocation_requests_) {
      if (req.id == id && req.client_id == client_id) {
        gfx::GpuMemoryBufferHandle handle;
        handle.id = id;
        handle.type = gfx::SHARED_MEMORY_BUFFER;
        req.callback.Run(handle);
        return;
      }
    }
    NOTREACHED();
  }

  // ui::mojom::GpuService:
  void EstablishGpuChannel(
      int32_t client_id,
      uint64_t client_tracing_id,
      bool is_gpu_host,
      const EstablishGpuChannelCallback& callback) override {}

  void CloseChannel(int32_t client_id) override {}

  void CreateGpuMemoryBuffer(
      gfx::GpuMemoryBufferId id,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      int client_id,
      gpu::SurfaceHandle surface_handle,
      const CreateGpuMemoryBufferCallback& callback) override {
    allocation_requests_.push_back(
        {id, size, format, usage, client_id, callback});
  }

  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id,
                              const gpu::SyncToken& sync_token) override {
    destruction_requests_.push_back({id, client_id});
  }

  void GetVideoMemoryUsageStats(
      const GetVideoMemoryUsageStatsCallback& callback) override {}

  void RequestCompleteGpuInfo(
      const RequestCompleteGpuInfoCallback& callback) override {}

  void LoadedShader(const std::string& data) override {}

  void DestroyingVideoSurface(
      int32_t surface_id,
      const DestroyingVideoSurfaceCallback& callback) override {}

  void WakeUpGpu() override {}

  void GpuSwitched() override {}

  void DestroyAllChannels() override {}

  void Crash() override {}

  void Hang() override {}

  void ThrowJavaException() override {}

  void Stop(const StopCallback& callback) override {}

 private:
  struct AllocationRequest {
    const gfx::GpuMemoryBufferId id;
    const gfx::Size size;
    const gfx::BufferFormat format;
    const gfx::BufferUsage usage;
    const int client_id;
    const CreateGpuMemoryBufferCallback callback;
  };
  std::vector<AllocationRequest> allocation_requests_;

  struct DestructionRequest {
    const gfx::GpuMemoryBufferId id;
    const int client_id;
  };
  std::vector<DestructionRequest> destruction_requests_;

  DISALLOW_COPY_AND_ASSIGN(TestGpuService);
};

// It is necessary to install a custom pixmap factory which claims to support
// all native configurations, so that code that deals with this can be tested
// correctly.
class FakeClientNativePixmapFactory : public gfx::ClientNativePixmapFactory {
 public:
  FakeClientNativePixmapFactory() {}
  ~FakeClientNativePixmapFactory() override {}

  // gfx::ClientNativePixmapFactory:
  bool IsConfigurationSupported(gfx::BufferFormat format,
                                gfx::BufferUsage usage) const override {
    return true;
  }
  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      const gfx::NativePixmapHandle& handle,
      const gfx::Size& size,
      gfx::BufferUsage usage) override {
    NOTREACHED();
    return nullptr;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeClientNativePixmapFactory);
};

}  // namespace

class ServerGpuMemoryBufferManagerTest : public ::testing::Test {
 public:
  ServerGpuMemoryBufferManagerTest() = default;
  ~ServerGpuMemoryBufferManagerTest() override = default;

  // ::testing::Test:
  void SetUp() override {
    gfx::ClientNativePixmapFactory::ResetInstance();
    gfx::ClientNativePixmapFactory::SetInstance(&pixmap_factory_);
  }

  void TearDown() override { gfx::ClientNativePixmapFactory::ResetInstance(); }

 private:
  base::test::ScopedTaskEnvironment env_;
  FakeClientNativePixmapFactory pixmap_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServerGpuMemoryBufferManagerTest);
};

// Tests that allocation requests from a client that goes away before allocation
// completes are cleaned up correctly.
TEST_F(ServerGpuMemoryBufferManagerTest, AllocationRequestsForDestroyedClient) {
#if !defined(USE_OZONE) && !defined(OS_MACOSX)
  // Not all platforms support native configurations (currently only ozone and
  // mac support it). Abort the test in those platforms.
  DCHECK(gpu::GetNativeGpuMemoryBufferConfigurations().empty());
  return;
#else
  // Note: ServerGpuMemoryBufferManager normally operates on a mojom::GpuService
  // implementation over mojo. Which means the communication from SGMBManager to
  // GpuService is asynchronous. In this test, the mojom::GpuService is not
  // bound to a mojo pipe, which means those calls are all synchronous.
  TestGpuService gpu_service;
  ServerGpuMemoryBufferManager manager(&gpu_service, 1);

  const auto buffer_id = static_cast<gfx::GpuMemoryBufferId>(1);
  const int client_id = 2;
  const gfx::Size size(10, 20);
  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;
  manager.AllocateGpuMemoryBuffer(
      buffer_id, client_id, size, format, usage, gpu::kNullSurfaceHandle,
      base::BindOnce([](const gfx::GpuMemoryBufferHandle& handle) {}));
  EXPECT_TRUE(gpu_service.HasAllocationRequest(buffer_id, client_id));
  EXPECT_FALSE(gpu_service.HasDestructionRequest(buffer_id, client_id));

  // Destroy the client. Since no memory has been allocated yet, there will be
  // no request for freeing memory.
  manager.DestroyAllGpuMemoryBufferForClient(client_id);
  EXPECT_TRUE(gpu_service.HasAllocationRequest(buffer_id, client_id));
  EXPECT_FALSE(gpu_service.HasDestructionRequest(buffer_id, client_id));

  // When the host receives the allocated memory for the destroyed client, it
  // should request the allocated memory to be freed.
  gpu_service.SatisfyAllocationRequest(buffer_id, client_id);
  EXPECT_TRUE(gpu_service.HasDestructionRequest(buffer_id, client_id));
#endif
}

}  // namespace viz
