// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/aura/surface_context_factory.h"

#include "cc/output/output_surface.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "ui/compositor/reflector.h"

namespace mojo {

SurfaceContextFactory::SurfaceContextFactory(Shell* shell, View* view)
    : surface_binding_(shell, view) {
}

SurfaceContextFactory::~SurfaceContextFactory() {
}

void SurfaceContextFactory::CreateOutputSurface(
    base::WeakPtr<ui::Compositor> compositor,
    bool software_fallback) {
  compositor->SetOutputSurface(surface_binding_.CreateOutputSurface());
}

scoped_refptr<ui::Reflector> SurfaceContextFactory::CreateReflector(
    ui::Compositor* mirroed_compositor,
    ui::Layer* mirroring_layer) {
  return new ui::Reflector();
}

void SurfaceContextFactory::RemoveReflector(
    scoped_refptr<ui::Reflector> reflector) {
}

scoped_refptr<cc::ContextProvider>
SurfaceContextFactory::SharedMainThreadContextProvider() {
  return nullptr;
}

void SurfaceContextFactory::RemoveCompositor(ui::Compositor* compositor) {
}

bool SurfaceContextFactory::DoesCreateTestContexts() {
  return false;
}

cc::SharedBitmapManager* SurfaceContextFactory::GetSharedBitmapManager() {
  return nullptr;
}

gpu::GpuMemoryBufferManager*
SurfaceContextFactory::GetGpuMemoryBufferManager() {
  return nullptr;
}

base::MessageLoopProxy* SurfaceContextFactory::GetCompositorMessageLoop() {
  return nullptr;
}

scoped_ptr<cc::SurfaceIdAllocator>
SurfaceContextFactory::CreateSurfaceIdAllocator() {
  return nullptr;
}

}  // namespace mojo
