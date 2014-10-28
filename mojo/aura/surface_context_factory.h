// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_AURA_SURFACE_CONTEXT_FACTORY_H_
#define MOJO_AURA_SURFACE_CONTEXT_FACTORY_H_

#include "mojo/aura/surface_binding.h"
#include "ui/compositor/compositor.h"

namespace mojo {
class Shell;
class View;

class SurfaceContextFactory : public ui::ContextFactory {
 public:
  SurfaceContextFactory(Shell* shell, View* view);
  ~SurfaceContextFactory() override;

 private:
  // ContextFactory:
  void CreateOutputSurface(base::WeakPtr<ui::Compositor> compositor,
                           bool software_fallback) override;
  scoped_refptr<ui::Reflector> CreateReflector(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer) override;
  void RemoveReflector(scoped_refptr<ui::Reflector> reflector) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  bool DoesCreateTestContexts() override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  base::MessageLoopProxy* GetCompositorMessageLoop() override;
  scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator() override;

  SurfaceBinding surface_binding_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceContextFactory);
};

}  // namespace mojo

#endif  // MOJO_AURA_SURFACE_CONTEXT_FACTORY_H_
