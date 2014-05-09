// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_AURA_CONTEXT_FACTORY_MOJO_H_
#define MOJO_AURA_CONTEXT_FACTORY_MOJO_H_

#include "mojo/public/cpp/system/core.h"
#include "ui/compositor/compositor.h"

namespace webkit {
namespace gpu {
class ContextProviderInProcess;
}
}

namespace mojo {

// The default factory that creates in-process contexts.
class ContextFactoryMojo : public ui::ContextFactory {
 public:
  explicit ContextFactoryMojo(ScopedMessagePipeHandle gles2_handle);
  virtual ~ContextFactoryMojo();

  // ContextFactory implementation
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      ui::Compositor* compositor, bool software_fallback) OVERRIDE;

  virtual scoped_refptr<ui::Reflector> CreateReflector(
      ui::Compositor* compositor,
      ui::Layer* layer) OVERRIDE;
  virtual void RemoveReflector(scoped_refptr<ui::Reflector> reflector) OVERRIDE;

  virtual scoped_refptr<cc::ContextProvider>
      SharedMainThreadContextProvider() OVERRIDE;
  virtual void RemoveCompositor(ui::Compositor* compositor) OVERRIDE;
  virtual bool DoesCreateTestContexts() OVERRIDE;
  virtual cc::SharedBitmapManager* GetSharedBitmapManager() OVERRIDE;

 private:
  scoped_refptr<webkit::gpu::ContextProviderInProcess>
      shared_main_thread_contexts_;

  ScopedMessagePipeHandle gles2_handle_;

  DISALLOW_COPY_AND_ASSIGN(ContextFactoryMojo);
};

}  // namespace mojo

#endif  // MOJO_AURA_CONTEXT_FACTORY_MOJO_H_
