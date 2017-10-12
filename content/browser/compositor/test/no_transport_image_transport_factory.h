// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_
#define CONTENT_BROWSER_COMPOSITOR_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/compositor/image_transport_factory.h"
#include "ui/compositor/test/in_process_context_factory.h"

namespace gl {
class DisableNullDrawGLBindings;
}

namespace ui {
class InProcessContextFactory;
}

namespace viz {
class ContextProvider;
}

namespace content {

// An ImageTransportFactory that disables transport.
class NoTransportImageTransportFactory : public ImageTransportFactory {
 public:
  NoTransportImageTransportFactory();
  ~NoTransportImageTransportFactory() override;

  // ImageTransportFactory implementation.
  ui::ContextFactory* GetContextFactory() override;
  ui::ContextFactoryPrivate* GetContextFactoryPrivate() override;
  viz::GLHelper* GetGLHelper() override;
  void SetGpuChannelEstablishFactory(
      gpu::GpuChannelEstablishFactory* factory) override;
#if defined(OS_MACOSX)
  void SetCompositorSuspendedForRecycle(ui::Compositor* compositor,
                                        bool suspended) override {}
#endif

 private:
  // The FrameSinkManagerImpl implementation lives in-process here for tests.
  viz::FrameSinkManagerImpl frame_sink_manager_;
  viz::HostFrameSinkManager host_frame_sink_manager_;
  ui::InProcessContextFactory context_factory_;
  scoped_refptr<viz::ContextProvider> context_provider_;
  std::unique_ptr<viz::GLHelper> gl_helper_;
  std::unique_ptr<gl::DisableNullDrawGLBindings> disable_null_draw_;

  DISALLOW_COPY_AND_ASSIGN(NoTransportImageTransportFactory);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_TEST_NO_TRANSPORT_IMAGE_TRANSPORT_FACTORY_H_
