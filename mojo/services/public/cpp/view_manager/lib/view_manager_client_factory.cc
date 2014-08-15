// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"

#include "mojo/services/public/cpp/view_manager/lib/view_manager_client_impl.h"

namespace mojo {

ViewManagerClientFactory::ViewManagerClientFactory(
    ViewManagerDelegate* delegate)
    : delegate_(delegate) {
}

ViewManagerClientFactory::~ViewManagerClientFactory() {
}

// InterfaceFactory<ViewManagerClient> implementation.
void ViewManagerClientFactory::Create(
    ApplicationConnection* connection,
    InterfaceRequest<ViewManagerClient> request) {
  BindToRequest(new ViewManagerClientImpl(delegate_, connection), &request);
}

}  // namespace mojo
