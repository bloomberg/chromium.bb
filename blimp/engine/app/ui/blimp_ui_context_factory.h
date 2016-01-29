// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_APP_UI_BLIMP_UI_CONTEXT_FACTORY_H_
#define BLIMP_ENGINE_APP_UI_BLIMP_UI_CONTEXT_FACTORY_H_

#include <stdint.h>

#include "base/macros.h"
#include "blimp/common/compositor/blimp_task_graph_runner.h"
#include "ui/compositor/compositor.h"

namespace blimp {
namespace engine {

class BlimpUiContextFactory : public ui::ContextFactory {
 public:
  BlimpUiContextFactory();
  ~BlimpUiContextFactory() override;

 private:
  // ui::ContextFactory implementation.
  void CreateOutputSurface(base::WeakPtr<ui::Compositor> compositor) override;
  scoped_ptr<ui::Reflector> CreateReflector(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer) override;
  void RemoveReflector(ui::Reflector* reflector) override;
  scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider() override;
  void RemoveCompositor(ui::Compositor* compositor) override;
  bool DoesCreateTestContexts() override;
  uint32_t GetImageTextureTarget(gfx::BufferFormat format,
                                 gfx::BufferUsage usage) override;
  cc::SharedBitmapManager* GetSharedBitmapManager() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;
  cc::TaskGraphRunner* GetTaskGraphRunner() override;
  scoped_ptr<cc::SurfaceIdAllocator> CreateSurfaceIdAllocator() override;
  void ResizeDisplay(ui::Compositor* compositor,
                     const gfx::Size& size) override;

  uint32_t next_surface_id_namespace_;
  BlimpTaskGraphRunner task_graph_runner_;

  DISALLOW_COPY_AND_ASSIGN(BlimpUiContextFactory);
};

}  // namespace engine
}  // namespace blimp

#endif  // BLIMP_ENGINE_APP_UI_BLIMP_UI_CONTEXT_FACTORY_H_
