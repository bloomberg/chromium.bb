// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/window_surface.h"

#include "components/mus/public/cpp/window_surface_client.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

namespace mus {

// static
scoped_ptr<WindowSurface> WindowSurface::Create(
    scoped_ptr<WindowSurfaceBinding>* surface_binding) {
  mojom::SurfacePtr surface;
  mojom::SurfaceClientPtr surface_client;
  mojo::InterfaceRequest<mojom::SurfaceClient> surface_client_request =
      GetProxy(&surface_client);

  surface_binding->reset(
      new WindowSurfaceBinding(GetProxy(&surface), surface_client.Pass()));
  return make_scoped_ptr(new WindowSurface(surface.PassInterface(),
                                           surface_client_request.Pass()));
}

WindowSurface::~WindowSurface() {}

void WindowSurface::BindToThread() {
  DCHECK(!thread_checker_);
  thread_checker_.reset(new base::ThreadChecker());
  surface_.Bind(surface_info_.Pass());
  client_binding_.reset(
      new mojo::Binding<mojom::SurfaceClient>(this, client_request_.Pass()));
}

void WindowSurface::SubmitCompositorFrame(mojom::CompositorFramePtr frame,
                                          const mojo::Closure& callback) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!surface_)
    return;
  surface_->SubmitCompositorFrame(frame.Pass(), callback);
}

WindowSurface::WindowSurface(
    mojo::InterfacePtrInfo<mojom::Surface> surface_info,
    mojo::InterfaceRequest<mojom::SurfaceClient> client_request)
    : client_(nullptr),
      surface_info_(surface_info.Pass()),
      client_request_(client_request.Pass()) {}

void WindowSurface::ReturnResources(
    mojo::Array<mojom::ReturnedResourcePtr> resources) {
  DCHECK(thread_checker_);
  DCHECK(thread_checker_->CalledOnValidThread());
  if (!client_)
    return;
  client_->OnResourcesReturned(this, resources.Pass());
}

WindowSurfaceBinding::~WindowSurfaceBinding() {}

WindowSurfaceBinding::WindowSurfaceBinding(
    mojo::InterfaceRequest<mojom::Surface> surface_request,
    mojom::SurfaceClientPtr surface_client)
    : surface_request_(surface_request.Pass()),
      surface_client_(surface_client.Pass()) {}

}  // namespace mus
