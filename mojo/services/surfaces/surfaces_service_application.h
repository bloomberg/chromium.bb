// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_SURFACES_SURFACES_SERVICE_APPLICATION_H_
#define MOJO_SERVICES_SURFACES_SURFACES_SERVICE_APPLICATION_H_

#include "base/macros.h"
#include "base/timer/timer.h"
#include "cc/surfaces/surface_manager.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "mojo/services/surfaces/surfaces_impl.h"

namespace mojo {
class ApplicationConnection;

class SurfacesServiceApplication : public ApplicationDelegate,
                                   public InterfaceFactory<SurfacesService>,
                                   public SurfacesImpl::Client {
 public:
  SurfacesServiceApplication();
  virtual ~SurfacesServiceApplication();

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE;

  // InterfaceFactory<SurfacsServicee> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<SurfacesService> request) OVERRIDE;

  // SurfacesImpl::Client implementation.
  virtual void FrameSubmitted() OVERRIDE;
  virtual void SetDisplay(cc::Display*) OVERRIDE;

 private:
  cc::SurfaceManager manager_;
  uint32_t next_id_namespace_;
  cc::Display* display_;
  // TODO(jamesr): Integrate with real scheduler.
  base::Timer draw_timer_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesServiceApplication);
};

}  // namespace mojo

#endif  //  MOJO_SERVICES_SURFACES_SURFACES_SERVICE_APPLICATION_H_
