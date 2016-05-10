// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_compositor_dependencies.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/buffer_types.h"

namespace content {

FakeCompositorDependencies::FakeCompositorDependencies() {
}

FakeCompositorDependencies::~FakeCompositorDependencies() {
}

bool FakeCompositorDependencies::IsGpuRasterizationForced() {
  return false;
}

bool FakeCompositorDependencies::IsGpuRasterizationEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsAsyncWorkerContextEnabled() {
  return false;
}

int FakeCompositorDependencies::GetGpuRasterizationMSAASampleCount() {
  return 0;
}

bool FakeCompositorDependencies::IsLcdTextEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsDistanceFieldTextEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsZeroCopyEnabled() {
  return true;
}

bool FakeCompositorDependencies::IsPartialRasterEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsGpuMemoryBufferCompositorResourcesEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsElasticOverscrollEnabled() {
  return true;
}
std::vector<unsigned> FakeCompositorDependencies::GetImageTextureTargets() {
  return std::vector<unsigned>(static_cast<size_t>(gfx::BufferFormat::LAST) + 1,
                               GL_TEXTURE_2D);
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCompositorDependencies::GetCompositorMainThreadTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCompositorDependencies::GetCompositorImplThreadTaskRunner() {
  return nullptr;  // Currently never threaded compositing in unit tests.
}

cc::SharedBitmapManager* FakeCompositorDependencies::GetSharedBitmapManager() {
  return &shared_bitmap_manager_;
}

gpu::GpuMemoryBufferManager*
FakeCompositorDependencies::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

scheduler::RendererScheduler*
FakeCompositorDependencies::GetRendererScheduler() {
  return &renderer_scheduler_;
}

std::unique_ptr<cc::BeginFrameSource>
FakeCompositorDependencies::CreateExternalBeginFrameSource(int routing_id) {
  double refresh_rate = 200.0;
  return base::WrapUnique(new cc::FakeExternalBeginFrameSource(refresh_rate));
}

cc::ImageSerializationProcessor*
FakeCompositorDependencies::GetImageSerializationProcessor() {
  return nullptr;
}

cc::TaskGraphRunner* FakeCompositorDependencies::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

bool FakeCompositorDependencies::AreImageDecodeTasksEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsThreadedAnimationEnabled() {
  return true;
}

}  // namespace content
