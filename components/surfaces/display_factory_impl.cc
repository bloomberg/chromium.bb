// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surfaces/display_factory_impl.h"

#include "cc/surfaces/surface_id.h"

namespace surfaces {

DisplayFactoryImpl::DisplayFactoryImpl(
    SurfacesServiceApplication* application,
    cc::SurfaceManager* manager,
    uint32_t id_namespace,
    SurfacesScheduler* scheduler,
    mojo::InterfaceRequest<mojo::DisplayFactory> request)
    : id_namespace_(id_namespace),
      next_local_id_(1u),
      application_(application),
      scheduler_(scheduler),
      manager_(manager),
      binding_(this, request.Pass()) {
}

DisplayFactoryImpl::~DisplayFactoryImpl() {
}

void DisplayFactoryImpl::Create(
    mojo::ContextProviderPtr context_provider,
    mojo::ResourceReturnerPtr returner,
    mojo::InterfaceRequest<mojo::Display> display_request) {
  cc::SurfaceId cc_id(static_cast<uint64_t>(id_namespace_) << 32 |
                      next_local_id_++);
  new DisplayImpl(application_, manager_, cc_id, scheduler_,
                  context_provider.Pass(), returner.Pass(),
                  display_request.Pass());
}

}  // namespace surfaces
