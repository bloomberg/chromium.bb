// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/fake_compositor_dependencies.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
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

const cc::BufferToTextureTargetMap&
FakeCompositorDependencies::GetBufferToTextureTargetMap() {
  return buffer_to_texture_target_map_;
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCompositorDependencies::GetCompositorMainThreadTaskRunner() {
  return base::ThreadTaskRunnerHandle::Get();
}

scoped_refptr<base::SingleThreadTaskRunner>
FakeCompositorDependencies::GetCompositorImplThreadTaskRunner() {
  return nullptr;  // Currently never threaded compositing in unit tests.
}

blink::scheduler::RendererScheduler*
FakeCompositorDependencies::GetRendererScheduler() {
  return &renderer_scheduler_;
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

bool FakeCompositorDependencies::IsScrollAnimatorEnabled() {
  return false;
}

}  // namespace content
