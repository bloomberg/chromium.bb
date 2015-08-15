// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_SERVICE_APPLICATION_H_
#define COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_SERVICE_APPLICATION_H_

#include <set>

#include "base/macros.h"
#include "cc/surfaces/surface_manager.h"
#include "components/view_manager/public/interfaces/display.mojom.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "components/view_manager/surfaces/display_delegate.h"
#include "components/view_manager/surfaces/surfaces_delegate.h"
#include "components/view_manager/surfaces/surfaces_state.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/interface_factory.h"
#include "mojo/common/tracing_impl.h"

namespace mojo {
class ApplicationConnection;
}

namespace surfaces {
class DisplayImpl;
class SurfacesImpl;

class SurfacesServiceApplication
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::DisplayFactory>,
      public mojo::InterfaceFactory<mojo::Surface>,
      public DisplayDelegate,
      public SurfacesDelegate {
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

 private:
  mojo::TracingImpl tracing_;

  // DisplayDelegate implementation.
  void OnDisplayCreated(DisplayImpl* display) override;
  void OnDisplayConnectionClosed(DisplayImpl* display) override;

  // SurfacesDelegate implementation.
  void OnSurfaceConnectionClosed(SurfacesImpl* surface) override;

  scoped_refptr<SurfacesState> surfaces_state_;

  std::set<DisplayImpl*> displays_;
  std::set<SurfacesImpl*> surfaces_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesServiceApplication);
};

}  // namespace surfaces

#endif  //  COMPONENTS_VIEW_MANAGER_SURFACES_SURFACES_SERVICE_APPLICATION_H_
