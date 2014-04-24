// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_AURA_DEMO_DEMO_CONTEXT_FACTORY_H_
#define MOJO_EXAMPLES_AURA_DEMO_DEMO_CONTEXT_FACTORY_H_

#include "ui/compositor/compositor.h"

namespace webkit {
namespace gpu {
class ContextProviderInProcess;
}
}

namespace mojo {
namespace examples {

class WindowTreeHostMojo;

// The default factory that creates in-process contexts.
class DemoContextFactory : public ui::ContextFactory {
 public:
  explicit DemoContextFactory(WindowTreeHostMojo* rwhm);
  virtual ~DemoContextFactory();

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

  bool Initialize();

 private:
  scoped_refptr<webkit::gpu::ContextProviderInProcess>
      shared_main_thread_contexts_;

  WindowTreeHostMojo* rwhm_;

  DISALLOW_COPY_AND_ASSIGN(DemoContextFactory);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_DEMO_CONTEXT_FACTORY_H_
