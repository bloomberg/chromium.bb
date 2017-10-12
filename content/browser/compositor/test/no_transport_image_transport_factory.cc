// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/test/no_transport_image_transport_factory.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/gl_helper.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gl/gl_implementation.h"

namespace content {

NoTransportImageTransportFactory::NoTransportImageTransportFactory()
    : context_factory_(&host_frame_sink_manager_, &frame_sink_manager_) {
  surface_utils::ConnectWithLocalFrameSinkManager(&host_frame_sink_manager_,
                                                  &frame_sink_manager_);

  // The context factory created here is for unit tests, thus using a higher
  // refresh rate to spend less time waiting for BeginFrames.
  context_factory_.SetUseFastRefreshRateForTests();

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnablePixelOutputInTests))
    disable_null_draw_ = std::make_unique<gl::DisableNullDrawGLBindings>();
}

NoTransportImageTransportFactory::~NoTransportImageTransportFactory() {
  std::unique_ptr<viz::GLHelper> lost_gl_helper = std::move(gl_helper_);
  context_factory_.SendOnLostResources();
}

ui::ContextFactory* NoTransportImageTransportFactory::GetContextFactory() {
  return &context_factory_;
}

ui::ContextFactoryPrivate*
NoTransportImageTransportFactory::GetContextFactoryPrivate() {
  return &context_factory_;
}

viz::GLHelper* NoTransportImageTransportFactory::GetGLHelper() {
  if (!gl_helper_) {
    context_provider_ = context_factory_.SharedMainThreadContextProvider();
    gl_helper_.reset(new viz::GLHelper(context_provider_->ContextGL(),
                                       context_provider_->ContextSupport()));
  }
  return gl_helper_.get();
}

void NoTransportImageTransportFactory::SetGpuChannelEstablishFactory(
    gpu::GpuChannelEstablishFactory* factory) {}

}  // namespace content
