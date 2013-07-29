// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/no_transport_image_transport_factory.h"
#include "ui/compositor/compositor.h"

namespace content {

NoTransportImageTransportFactory::NoTransportImageTransportFactory(
    ui::ContextFactory* context_factory)
    : context_factory_(context_factory) {}

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
  return NULL;
}

scoped_refptr<ui::Texture> NoTransportImageTransportFactory::CreateOwnedTexture(
    const gfx::Size& size,
    float device_scale_factor,
    unsigned int texture_id) {
  return NULL;
}

GLHelper* NoTransportImageTransportFactory::GetGLHelper() { return NULL; }

uint32 NoTransportImageTransportFactory::InsertSyncPoint() { return 0; }

void NoTransportImageTransportFactory::WaitSyncPoint(uint32 sync_point) {}

// We don't generate lost context events, so we don't need to keep track of
// observers
void NoTransportImageTransportFactory::AddObserver(
    ImageTransportFactoryObserver* observer) {}

void NoTransportImageTransportFactory::RemoveObserver(
    ImageTransportFactoryObserver* observer) {}

}  // namespace content
