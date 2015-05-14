// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/public/cpp/view_manager_client_factory.h"

#include "components/view_manager/public/cpp/lib/view_manager_client_impl.h"
#include "mojo/application/public/interfaces/shell.mojom.h"

namespace mojo {

ViewManagerClientFactory::ViewManagerClientFactory(
    Shell* shell,
    ViewManagerDelegate* delegate)
    : shell_(shell), delegate_(delegate) {
}

ViewManagerClientFactory::~ViewManagerClientFactory() {
}

// InterfaceFactory<ViewManagerClient> implementation.
void ViewManagerClientFactory::Create(
    ApplicationConnection* connection,
    InterfaceRequest<ViewManagerClient> request) {
  const bool delete_on_error = true;
  new ViewManagerClientImpl(delegate_, shell_, request.Pass(), delete_on_error);
}

}  // namespace mojo
