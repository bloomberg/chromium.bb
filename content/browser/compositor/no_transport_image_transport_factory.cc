// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/no_transport_image_transport_factory.h"

#include "cc/output/context_provider.h"
#include "content/common/gpu/client/gl_helper.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "ui/compositor/compositor.h"

namespace content {

namespace {

class FakeTexture : public ui::Texture {
 public:
  FakeTexture(scoped_refptr<cc::ContextProvider> context_provider,
              float device_scale_factor)
      : ui::Texture(false, gfx::Size(), device_scale_factor),
        context_provider_(context_provider),
        texture_(0u) {
    context_provider_->ContextGL()->GenTextures(1, &texture_);
  }

  virtual unsigned int PrepareTexture() OVERRIDE { return texture_; }

  virtual void Consume(const gpu::Mailbox& mailbox,
                       const gfx::Size& new_size) OVERRIDE {
    size_ = new_size;
  }

 private:
  virtual ~FakeTexture() {
    context_provider_->ContextGL()->DeleteTextures(1, &texture_);
  }

  scoped_refptr<cc::ContextProvider> context_provider_;
  unsigned texture_;
  DISALLOW_COPY_AND_ASSIGN(FakeTexture);
};

}  // anonymous namespace

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

scoped_refptr<ui::Texture>
NoTransportImageTransportFactory::CreateTransportClient(
    float device_scale_factor) {
  return new FakeTexture(context_factory_->SharedMainThreadContextProvider(),
                         device_scale_factor);
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
