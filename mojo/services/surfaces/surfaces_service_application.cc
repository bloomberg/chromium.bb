// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/surfaces/surfaces_service_application.h"

#include "cc/surfaces/display.h"

namespace mojo {
namespace surfaces {

SurfacesServiceApplication::SurfacesServiceApplication()
    : next_id_namespace_(1u), display_(NULL) {
}

SurfacesServiceApplication::~SurfacesServiceApplication() {
}

bool SurfacesServiceApplication::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<SurfacesImpl, SurfacesImpl::Context>(this);
  return true;
}

cc::SurfaceManager* SurfacesServiceApplication::Manager() {
  return &manager_;
}

uint32_t SurfacesServiceApplication::IdNamespace() {
  return next_id_namespace_++;
}

void SurfacesServiceApplication::FrameSubmitted() {
  if (display_)
    display_->Draw();
}

void SurfacesServiceApplication::SetDisplay(cc::Display* display) {
  display_ = display;
}

}  // namespace surfaces

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new surfaces::SurfacesServiceApplication;
}

}  // namespace mojo
