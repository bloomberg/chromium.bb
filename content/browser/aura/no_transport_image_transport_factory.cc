// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/no_transport_image_transport_factory.h"

#include "cc/output/context_provider.h"
#include "content/common/gpu/client/gl_helper.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "ui/compositor/compositor.h"

namespace content {

namespace {

class FakeTexture : public ui::Texture {
 public:
  FakeTexture(scoped_refptr<cc::ContextProvider> context_provider,
              float device_scale_factor)
      : ui::Texture(false, gfx::Size(), device_scale_factor),
        context_provider_(context_provider),
        texture_(context_provider_->Context3d()->createTexture()) {}

  virtual unsigned int PrepareTexture() OVERRIDE { return texture_; }

  virtual void Consume(const std::string& mailbox_name,
                       const gfx::Size& new_size) OVERRIDE {
    size_ = new_size;
  }

 private:
  virtual ~FakeTexture() {
    context_provider_->Context3d()->deleteTexture(texture_);
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
NoTransportImageTransportFactory::CreateSharedSurfaceHandle() {
  return gfx::GLSurfaceHandle();
}

void NoTransportImageTransportFactory::DestroySharedSurfaceHandle(
    gfx::GLSurfaceHandle surface) {}

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
    gl_helper_.reset(new GLHelper(context_provider_->Context3d(),
                                  context_provider_->ContextSupport()));
  }
  return gl_helper_.get();
}

uint32 NoTransportImageTransportFactory::InsertSyncPoint() { return 0; }

void NoTransportImageTransportFactory::WaitSyncPoint(uint32 sync_point) {}

// We don't generate lost context events, so we don't need to keep track of
// observers
void NoTransportImageTransportFactory::AddObserver(
    ImageTransportFactoryObserver* observer) {}

void NoTransportImageTransportFactory::RemoveObserver(
    ImageTransportFactoryObserver* observer) {}

}  // namespace content
