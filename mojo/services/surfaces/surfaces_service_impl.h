// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_SURFACES_SURFACES_SERVICE_IMPL_H_
#define MOJO_SERVICES_SURFACES_SURFACES_SERVICE_IMPL_H_

#include "base/macros.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "mojo/services/surfaces/surfaces_impl.h"

namespace cc {
class SurfaceManager;
}

namespace mojo {

class SurfacesServiceImpl : public InterfaceImpl<SurfacesService> {
 public:
  // The instances pointed to by |manager|, |next_id_namespace| and |client| are
  // owned by the caller and must outlive the SurfacesServiceImpl instance.
  SurfacesServiceImpl(cc::SurfaceManager* manager,
                      uint32_t* next_id_namespace,
                      SurfacesImpl::Client* client);
  virtual ~SurfacesServiceImpl();

  // InterfaceImpl<SurfacesService> implementation.
  virtual void CreateSurfaceConnection(const mojo::Callback<
      void(mojo::SurfacePtr, uint32_t)>& callback) OVERRIDE;

 private:
  cc::SurfaceManager* manager_;
  uint32_t* next_id_namespace_;
  SurfacesImpl::Client* client_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesServiceImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_SURFACES_SURFACES_SERVICE_IMPL_H_
