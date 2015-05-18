// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SURFACES_SURFACES_SERVICE_APPLICATION_H_
#define COMPONENTS_SURFACES_SURFACES_SERVICE_APPLICATION_H_

#include <set>

#include "base/macros.h"
#include "cc/surfaces/surface_manager.h"
#include "components/surfaces/public/interfaces/display.mojom.h"
#include "components/surfaces/public/interfaces/surfaces.mojom.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/tracing_impl.h"

namespace mojo {
class ApplicationConnection;
}

namespace surfaces {
class DisplayImpl;
class SurfacesImpl;
class SurfacesScheduler;

class SurfacesServiceApplication
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::DisplayFactory>,
      public mojo::InterfaceFactory<mojo::Surface> {
 public:
  SurfacesServiceApplication();
  ~SurfacesServiceApplication() override;

  // ApplicationDelegate implementation.
  void Initialize(mojo::ApplicationImpl* app) override;
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // InterfaceFactory<DisplayFactory> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::DisplayFactory> request) override;

  // InterfaceFactory<Surface> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::Surface> request) override;

  void DisplayCreated(DisplayImpl* display);
  void DisplayDestroyed(DisplayImpl* display);
  void SurfaceDestroyed(SurfacesImpl* surface);

 private:
  cc::SurfaceManager manager_;
  uint32_t next_id_namespace_;
  scoped_ptr<SurfacesScheduler> scheduler_;
  mojo::TracingImpl tracing_;

  // Since these two classes have non-owning pointers to |manager_|, need to
  // destroy them if this class is destructed first.
  std::set<DisplayImpl*> displays_;
  std::set<SurfacesImpl*> surfaces_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesServiceApplication);
};

}  // namespace surfaces

#endif  //  COMPONENTS_SURFACES_SURFACES_SERVICE_APPLICATION_H_
