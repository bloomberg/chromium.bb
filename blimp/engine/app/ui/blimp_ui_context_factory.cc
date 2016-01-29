// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/ui/blimp_ui_context_factory.h"

#include "cc/output/output_surface.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/compositor/reflector.h"

namespace blimp {
namespace engine {

BlimpUiContextFactory::BlimpUiContextFactory()
    : next_surface_id_namespace_(1u) {}

BlimpUiContextFactory::~BlimpUiContextFactory() {}

void BlimpUiContextFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor) {
  NOTIMPLEMENTED();
}

scoped_ptr<ui::Reflector> BlimpUiContextFactory::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  NOTREACHED();
  return nullptr;
}

void BlimpUiContextFactory::RemoveReflector(ui::Reflector* reflector) {
  NOTREACHED();
}

scoped_refptr<cc::ContextProvider>
BlimpUiContextFactory::SharedMainThreadContextProvider() {
  NOTREACHED();
  return nullptr;
}

void BlimpUiContextFactory::RemoveCompositor(ui::Compositor* compositor) {
  NOTIMPLEMENTED();
}

bool BlimpUiContextFactory::DoesCreateTestContexts() {
  return false;
}

uint32_t BlimpUiContextFactory::GetImageTextureTarget(gfx::BufferFormat format,
                                                      gfx::BufferUsage usage) {
  // No GpuMemoryBuffer support, so just return GL_TEXTURE_2D.
  return GL_TEXTURE_2D;
}

cc::SharedBitmapManager* BlimpUiContextFactory::GetSharedBitmapManager() {
  NOTIMPLEMENTED();
  return nullptr;
}

gpu::GpuMemoryBufferManager*
BlimpUiContextFactory::GetGpuMemoryBufferManager() {
  return nullptr;
}

cc::TaskGraphRunner* BlimpUiContextFactory::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

scoped_ptr<cc::SurfaceIdAllocator>
BlimpUiContextFactory::CreateSurfaceIdAllocator() {
  return make_scoped_ptr(
      new cc::SurfaceIdAllocator(next_surface_id_namespace_++));
}

void BlimpUiContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                          const gfx::Size& size) {
  NOTIMPLEMENTED();
}

}  // namespace engine
}  // namespace blimp
