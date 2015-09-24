// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/view_surface.h"

#include "components/mus/public/cpp/view_surface_client.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

namespace mus {

ViewSurface::~ViewSurface() {}

void ViewSurface::BindToThread() {
  DCHECK(!bound_to_thread_);
  bound_to_thread_ = true;
  surface_.Bind(surface_info_.Pass());
  client_binding_.Bind(client_request_.Pass());
}

void ViewSurface::SubmitCompositorFrame(mojo::CompositorFramePtr frame,
                                        const mojo::Closure& callback) {
  DCHECK(bound_to_thread_);
  if (!surface_)
    return;
  surface_->SubmitCompositorFrame(frame.Pass(), callback);
}

ViewSurface::ViewSurface(
    mojo::InterfacePtrInfo<mojo::Surface> surface_info,
    mojo::InterfaceRequest<mojo::SurfaceClient> client_request)
    : client_(nullptr),
      surface_info_(surface_info.Pass()),
      client_request_(client_request.Pass()),
      client_binding_(this),
      bound_to_thread_(false) {}

void ViewSurface::ReturnResources(
    mojo::Array<mojo::ReturnedResourcePtr> resources) {
  if (!client_)
    return;
  client_->OnResourcesReturned(this, resources.Pass());
}

}  // namespace mus
