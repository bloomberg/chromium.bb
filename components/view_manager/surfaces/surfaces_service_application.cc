// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/surfaces/surfaces_service_application.h"

#include "base/stl_util.h"
#include "components/view_manager/surfaces/display_factory_impl.h"
#include "components/view_manager/surfaces/surfaces_impl.h"
#include "components/view_manager/surfaces/surfaces_state.h"

namespace surfaces {

SurfacesServiceApplication::SurfacesServiceApplication() {
}

SurfacesServiceApplication::~SurfacesServiceApplication() {
  // Make a copy of the sets before deleting them because their destructor will
  // call back into this class to remove them from the set.
  auto displays = displays_;
  for (auto& display : displays)
    display->CloseConnection();

  auto surfaces = surfaces_;
  for (auto& surface : surfaces)
    surface->CloseConnection();
}

void SurfacesServiceApplication::Initialize(mojo::ApplicationImpl* app) {
  tracing_.Initialize(app);
  surfaces_state_ = new SurfacesState;
}

bool SurfacesServiceApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<mojo::DisplayFactory>(this);
  connection->AddService<mojo::Surface>(this);
  return true;
}

void SurfacesServiceApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::DisplayFactory> request) {
  new DisplayFactoryImpl(this, surfaces_state_, request.Pass());
}

void SurfacesServiceApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mojo::Surface> request) {
  // SurfacesImpl manages its own lifetime.
  surfaces_.insert(new SurfacesImpl(this, surfaces_state_, request.Pass()));
}

void SurfacesServiceApplication::OnDisplayCreated(DisplayImpl* display) {
  displays_.insert(display);
}

void SurfacesServiceApplication::OnDisplayConnectionClosed(
    DisplayImpl* display) {
  displays_.erase(display);
}

void SurfacesServiceApplication::OnSurfaceConnectionClosed(
    SurfacesImpl* surface) {
  surfaces_.erase(surface);
}

}  // namespace surfaces
