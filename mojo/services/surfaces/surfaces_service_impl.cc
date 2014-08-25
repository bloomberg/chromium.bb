// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/surfaces/surfaces_service_impl.h"

namespace mojo {

SurfacesServiceImpl::SurfacesServiceImpl(cc::SurfaceManager* manager,
                                         uint32_t* next_id_namespace,
                                         SurfacesImpl::Client* client)
    : manager_(manager),
      next_id_namespace_(next_id_namespace),
      client_(client) {
}
SurfacesServiceImpl::~SurfacesServiceImpl() {
}

void SurfacesServiceImpl::CreateSurfaceConnection(
    const Callback<void(SurfacePtr, uint32_t)>& callback) {
  uint32_t id_namespace = (*next_id_namespace_)++;
  SurfacePtr surface;
  BindToProxy(new SurfacesImpl(manager_, id_namespace, client_), &surface);
  callback.Run(surface.Pass(), id_namespace);
}

}  // namespace mojo
