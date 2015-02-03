// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_compositor_dependencies.h"

#include "base/message_loop/message_loop_proxy.h"
#include "cc/test/fake_external_begin_frame_source.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace content {

FakeCompositorDependencies::FakeCompositorDependencies()
    : use_single_thread_scheduler_(true) {
}

bool FakeCompositorDependencies::IsImplSidePaintingEnabled() {
  return true;
}

bool FakeCompositorDependencies::IsGpuRasterizationForced() {
  return false;
}

bool FakeCompositorDependencies::IsGpuRasterizationEnabled() {
  return false;
}

bool FakeCompositorDependencies::IsThreadedGpuRasterizationEnabled() {
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
  return false;
}

bool FakeCompositorDependencies::IsOneCopyEnabled() {
  return true;
}

bool FakeCompositorDependencies::IsElasticOverscrollEnabled() {
  return false;
}

bool FakeCompositorDependencies::UseSingleThreadScheduler() {
  return use_single_thread_scheduler_;
}

uint32 FakeCompositorDependencies::GetImageTextureTarget() {
  return GL_TEXTURE_2D;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCompositorDependencies::GetCompositorMainThreadTaskRunner() {
  return base::MessageLoopProxy::current();
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

RendererScheduler* FakeCompositorDependencies::GetRendererScheduler() {
  return &renderer_scheduler_;
}

cc::ContextProvider*
FakeCompositorDependencies::GetSharedMainThreadContextProvider() {
  NOTREACHED();
  return nullptr;
}

scoped_ptr<cc::BeginFrameSource>
FakeCompositorDependencies::CreateExternalBeginFrameSource(int routing_id) {
  double refresh_rate = 200.0;
  return make_scoped_ptr(new cc::FakeExternalBeginFrameSource(refresh_rate));
}

}  // namespace content
