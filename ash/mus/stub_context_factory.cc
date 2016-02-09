// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/stub_context_factory.h"

#include "cc/output/context_provider.h"
#include "cc/raster/single_thread_task_graph_runner.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/reflector.h"

namespace ash {
namespace sysui {

namespace {

class StubTaskGraphRunner : public cc::SingleThreadTaskGraphRunner {
 public:
  StubTaskGraphRunner() {
    Start("CompositorTileWorker1", base::SimpleThread::Options());
  }
  ~StubTaskGraphRunner() override {}
};

}  // namespace

StubContextFactory::StubContextFactory()
    : next_surface_id_namespace_(1u),
      task_graph_runner_(new StubTaskGraphRunner) {}

StubContextFactory::~StubContextFactory() {}

void StubContextFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor) {}

scoped_ptr<ui::Reflector> StubContextFactory::CreateReflector(
    ui::Compositor* mirrored_compositor,
    ui::Layer* mirroring_layer) {
  return nullptr;
}

void StubContextFactory::RemoveReflector(ui::Reflector* reflector) {}

scoped_refptr<cc::ContextProvider>
StubContextFactory::SharedMainThreadContextProvider() {
  return nullptr;
}

void StubContextFactory::RemoveCompositor(ui::Compositor* compositor) {}

bool StubContextFactory::DoesCreateTestContexts() {
  return false;
}

uint32_t StubContextFactory::GetImageTextureTarget(gfx::BufferFormat format,
                                                   gfx::BufferUsage usage) {
  return GL_TEXTURE_2D;
}

cc::SharedBitmapManager* StubContextFactory::GetSharedBitmapManager() {
  return nullptr;
}

gpu::GpuMemoryBufferManager* StubContextFactory::GetGpuMemoryBufferManager() {
  return nullptr;
}

cc::TaskGraphRunner* StubContextFactory::GetTaskGraphRunner() {
  return task_graph_runner_.get();
}

scoped_ptr<cc::SurfaceIdAllocator>
StubContextFactory::CreateSurfaceIdAllocator() {
  return make_scoped_ptr(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
}

void StubContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                       const gfx::Size& size) {}

}  // namespace sysui
}  // namespace ash
