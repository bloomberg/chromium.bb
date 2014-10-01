// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_AURA_CONTEXT_FACTORY_MOJO_H_
#define MOJO_AURA_CONTEXT_FACTORY_MOJO_H_

#include "base/macros.h"
#include "ui/compositor/compositor.h"

namespace mojo {

class ContextFactoryMojo : public ui::ContextFactory {
 public:
  ContextFactoryMojo();
  virtual ~ContextFactoryMojo();

 private:
  // ContextFactory:
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      ui::Compositor* compositor,
      bool software_fallback) override;
  virtual scoped_refptr<ui::Reflector> CreateReflector(
      ui::Compositor* mirrored_compositor,
      ui::Layer* mirroring_layer) override;
  virtual void RemoveReflector(scoped_refptr<ui::Reflector> reflector) override;
  virtual scoped_refptr<cc::ContextProvider> SharedMainThreadContextProvider()
      override;
  virtual void RemoveCompositor(ui::Compositor* compositor) override;
  virtual bool DoesCreateTestContexts() override;
  virtual cc::SharedBitmapManager* GetSharedBitmapManager() override;
  virtual base::MessageLoopProxy* GetCompositorMessageLoop() override;

  scoped_ptr<cc::SharedBitmapManager> shared_bitmap_manager_;

  DISALLOW_COPY_AND_ASSIGN(ContextFactoryMojo);
};

}  // namespace mojo

#endif  // MOJO_AURA_CONTEXT_FACTORY_MOJO_H_
