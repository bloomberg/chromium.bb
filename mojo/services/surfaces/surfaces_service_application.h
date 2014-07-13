// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_SURFACES_SURFACES_SERVICE_APPLICATION_H_
#define MOJO_SERVICES_SURFACES_SURFACES_SERVICE_APPLICATION_H_

#include "base/macros.h"
#include "cc/surfaces/surface_manager.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/services/surfaces/surfaces_impl.h"

namespace mojo {
class ApplicationConnection;

namespace surfaces {

class SurfacesServiceApplication : public ApplicationDelegate,
                                   public SurfacesImpl::Context {
 public:
  SurfacesServiceApplication();
  virtual ~SurfacesServiceApplication();

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE;

  // SurfacesImpl::Context implementation.
  virtual cc::SurfaceManager* Manager() OVERRIDE;
  virtual uint32_t IdNamespace() OVERRIDE;
  virtual void FrameSubmitted() OVERRIDE;
  virtual void SetDisplay(cc::Display*) OVERRIDE;

 private:
  cc::SurfaceManager manager_;
  uint32_t next_id_namespace_;
  cc::Display* display_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesServiceApplication);
};

}  // namespace surfaces
}  // namespace mojo

#endif  //  MOJO_SERVICES_SURFACES_SURFACES_SERVICE_APPLICATION_H_
