// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mandoline/tab/frame_services.h"

#include "mojo/application/public/cpp/connect.h"

namespace mandoline {

FrameServices::FrameServices() {
}

FrameServices::~FrameServices() {
}

void FrameServices::Init(
    mojo::InterfaceRequest<mojo::ServiceProvider>* services,
    mojo::ServiceProviderPtr* exposed_services) {
  *services = GetProxy(&services_).Pass();

  if (exposed_services) {
    mojo::ServiceProviderPtr exposed_services_ptr;
    exposed_services_.Bind(GetProxy(&exposed_services_ptr));
    *exposed_services = exposed_services_ptr.Pass();
  }

  mojo::ConnectToService(services_.get(), &frame_tree_client_);
}

}  // namespace mandoline
