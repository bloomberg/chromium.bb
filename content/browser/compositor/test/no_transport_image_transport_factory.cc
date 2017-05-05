// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/test/no_transport_image_transport_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "cc/output/context_provider.h"
#include "cc/surfaces/surface_manager.h"
#include "components/display_compositor/gl_helper.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/test/in_process_context_factory.h"

namespace content {

NoTransportImageTransportFactory::NoTransportImageTransportFactory()
    : frame_sink_manager_host_(base::MakeUnique<FrameSinkManagerHost>()),
      context_factory_(base::MakeUnique<ui::InProcessContextFactory>(
          frame_sink_manager_host_->surface_manager())) {
  // The context factory created here is for unit tests, thus using a higher
  // refresh rate to spend less time waiting for BeginFrames.
  context_factory_->SetUseFastRefreshRateForTests();
}

NoTransportImageTransportFactory::~NoTransportImageTransportFactory() {
  std::unique_ptr<display_compositor::GLHelper> lost_gl_helper =
      std::move(gl_helper_);
  context_factory_->SendOnLostResources();
}

ui::ContextFactory* NoTransportImageTransportFactory::GetContextFactory() {
  return context_factory_.get();
}

ui::ContextFactoryPrivate*
NoTransportImageTransportFactory::GetContextFactoryPrivate() {
  return context_factory_.get();
}

FrameSinkManagerHost*
NoTransportImageTransportFactory::GetFrameSinkManagerHost() {
  return frame_sink_manager_host_.get();
}

display_compositor::GLHelper* NoTransportImageTransportFactory::GetGLHelper() {
  if (!gl_helper_) {
    context_provider_ = context_factory_->SharedMainThreadContextProvider();
    gl_helper_.reset(new display_compositor::GLHelper(
        context_provider_->ContextGL(), context_provider_->ContextSupport()));
  }
  return gl_helper_.get();
}

void NoTransportImageTransportFactory::SetGpuChannelEstablishFactory(
    gpu::GpuChannelEstablishFactory* factory) {}

}  // namespace content
