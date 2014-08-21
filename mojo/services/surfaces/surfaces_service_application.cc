// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/surfaces/surfaces_service_application.h"

#include "cc/surfaces/display.h"

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_runner_chromium.h"

namespace mojo {

SurfacesServiceApplication::SurfacesServiceApplication()
    : next_id_namespace_(1u), display_(NULL) {
}

SurfacesServiceApplication::~SurfacesServiceApplication() {
}

bool SurfacesServiceApplication::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void SurfacesServiceApplication::Create(ApplicationConnection* connection,
                                        InterfaceRequest<Surface> request) {
  BindToRequest(new SurfacesImpl(&manager_, next_id_namespace_++, this),
                &request);
}

void SurfacesServiceApplication::FrameSubmitted() {
  if (display_)
    display_->Draw();
}

void SurfacesServiceApplication::SetDisplay(cc::Display* display) {
  display_ = display;
}

}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::SurfacesServiceApplication);
  return runner.Run(shell_handle);
}
