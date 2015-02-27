// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_

#include "cc/test/test_gpu_memory_buffer_manager.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "content/renderer/gpu/compositor_dependencies.h"
#include "content/test/fake_renderer_scheduler.h"

namespace content {

class FakeCompositorDependencies : public CompositorDependencies {
 public:
  FakeCompositorDependencies();

  // CompositorDependencies implementation.
  bool IsImplSidePaintingEnabled() override;
  bool IsGpuRasterizationForced() override;
  bool IsGpuRasterizationEnabled() override;
  bool IsThreadedGpuRasterizationEnabled() override;
  int GetGpuRasterizationMSAASampleCount() override;
  bool IsLcdTextEnabled() override;
  bool IsDistanceFieldTextEnabled() override;
  bool IsZeroCopyEnabled() override;
  bool IsOneCopyEnabled() override;
  bool IsElasticOverscrollEnabled() override;
  bool UseSingleThreadScheduler() override;
  uint32 GetImageTextureTarget() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() override;
  scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  RendererScheduler* GetRendererScheduler() override;
  cc::ContextProvider* GetSharedMainThreadContextProvider() override;
  scoped_ptr<cc::BeginFrameSource> CreateExternalBeginFrameSource(
      int routing_id) override;

  void set_use_single_thread_scheduler(bool use) {
    use_single_thread_scheduler_ = use;
  }

 private:
  cc::TestSharedBitmapManager shared_bitmap_manager_;
  cc::TestGpuMemoryBufferManager gpu_memory_buffer_manager_;
  FakeRendererScheduler renderer_scheduler_;
  bool use_single_thread_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorDependencies);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
