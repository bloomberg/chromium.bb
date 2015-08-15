// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/surfaces/display_factory_impl.h"

#include "cc/surfaces/surface_id.h"

namespace surfaces {

DisplayFactoryImpl::DisplayFactoryImpl(
    DisplayDelegate* display_delegate,
    const scoped_refptr<SurfacesState>& surfaces_state,
    mojo::InterfaceRequest<mojo::DisplayFactory> request)
    : id_namespace_(surfaces_state->next_id_namespace()),
      next_local_id_(1u),
      display_delegate_(display_delegate),
      surfaces_state_(surfaces_state),
      binding_(this, request.Pass()),
      connection_closed_(false) {
  binding_.set_connection_error_handler(
      base::Bind(&DisplayFactoryImpl::CloseConnection, base::Unretained(this)));
}

void DisplayFactoryImpl::CloseConnection() {
  if (connection_closed_)
    return;
  connection_closed_ = true;
  delete this;
}

void DisplayFactoryImpl::Create(
    mojo::ContextProviderPtr context_provider,
    mojo::ResourceReturnerPtr returner,
    mojo::InterfaceRequest<mojo::Display> display_request) {
  cc::SurfaceId cc_id(static_cast<uint64_t>(id_namespace_) << 32 |
                      next_local_id_++);
  new DisplayImpl(display_delegate_,
                  surfaces_state_, cc_id,
                  context_provider.Pass(),
                  returner.Pass(),
                  display_request.Pass());
}

DisplayFactoryImpl::~DisplayFactoryImpl() {
  // Only CloseConnection should be allowed to destroy this object.
  DCHECK(connection_closed_);
}

}  // namespace surfaces
