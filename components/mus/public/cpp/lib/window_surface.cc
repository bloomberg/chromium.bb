// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window_surface.h"

#include "components/mus/public/cpp/window_surface_client.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

namespace mus {

WindowSurface::~WindowSurface() {}

void WindowSurface::BindToThread() {
  DCHECK(!bound_to_thread_);
  bound_to_thread_ = true;
  surface_.Bind(surface_info_.Pass());
  client_binding_.reset(
      new mojo::Binding<mojo::SurfaceClient>(this, client_request_.Pass()));
}

void WindowSurface::SubmitCompositorFrame(mojo::CompositorFramePtr frame,
                                          const mojo::Closure& callback) {
  DCHECK(bound_to_thread_);
  if (!surface_)
    return;
  surface_->SubmitCompositorFrame(frame.Pass(), callback);
}

WindowSurface::WindowSurface(
    mojo::InterfacePtrInfo<mojo::Surface> surface_info,
    mojo::InterfaceRequest<mojo::SurfaceClient> client_request)
    : client_(nullptr),
      surface_info_(surface_info.Pass()),
      client_request_(client_request.Pass()),
      bound_to_thread_(false) {}

void WindowSurface::ReturnResources(
    mojo::Array<mojo::ReturnedResourcePtr> resources) {
  if (!client_)
    return;
  client_->OnResourcesReturned(this, resources.Pass());
}

}  // namespace mus
