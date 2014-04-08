// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/no_transport_image_transport_factory.h"

#include "cc/output/context_provider.h"
#include "content/common/gpu/client/gl_helper.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/compositor/compositor.h"

namespace content {

NoTransportImageTransportFactory::NoTransportImageTransportFactory(
    scoped_ptr<ui::ContextFactory> context_factory)
    : context_factory_(context_factory.Pass()) {}

NoTransportImageTransportFactory::~NoTransportImageTransportFactory() {}

ui::ContextFactory* NoTransportImageTransportFactory::AsContextFactory() {
  return context_factory_.get();
}

gfx::GLSurfaceHandle
NoTransportImageTransportFactory::GetSharedSurfaceHandle() {
  return gfx::GLSurfaceHandle();
}

scoped_refptr<ui::Texture> NoTransportImageTransportFactory::CreateOwnedTexture(
    const gfx::Size& size,
    float device_scale_factor,
    unsigned int texture_id) {
  return NULL;
}

GLHelper* NoTransportImageTransportFactory::GetGLHelper() {
  if (!gl_helper_) {
    context_provider_ = context_factory_->SharedMainThreadContextProvider();
    gl_helper_.reset(new GLHelper(context_provider_->ContextGL(),
                                  context_provider_->ContextSupport()));
  }
  return gl_helper_.get();
}

// We don't generate lost context events, so we don't need to keep track of
// observers
void NoTransportImageTransportFactory::AddObserver(
    ImageTransportFactoryObserver* observer) {}

void NoTransportImageTransportFactory::RemoveObserver(
    ImageTransportFactoryObserver* observer) {}

}  // namespace content
