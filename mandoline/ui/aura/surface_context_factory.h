// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MANDOLINE_UI_AURA_SURFACE_CONTEXT_FACTORY_H_
#define MANDOLINE_UI_AURA_SURFACE_CONTEXT_FACTORY_H_

#include "mandoline/ui/aura/surface_binding.h"
#include "ui/compositor/compositor.h"

namespace mojo {
class Shell;
class View;
}

namespace mandoline {

class SurfaceContextFactory : public ui::ContextFactory {
 public:
  SurfaceContextFactory(mojo::Shell* shell, mojo::View* view);
  ~SurfaceContextFactory() override;

 private:
  // ContextFactory:
  void CreateOutputSurface(base::WeakPtr<ui::Compositor> compositor) override;
  scoped_ptr<ui::Reflector> CreateReflector(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer) override;
  void RemoveReflector(ui::Reflector* reflector) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  bool DoesCreateTestContexts() override;
  uint32 GetImageTextureTarget() override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator() override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;

  SurfaceBinding surface_binding_;
  uint32_t next_surface_id_namespace_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceContextFactory);
};

}  // namespace mandoline

#endif  // MANDOLINE_UI_AURA_SURFACE_CONTEXT_FACTORY_H_
