// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/public/cpp/view_manager_client_factory.h"

#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/services/view_manager/public/cpp/lib/view_manager_client_impl.h"

namespace mojo {

ViewManagerClientFactory::ViewManagerClientFactory(
    Shell* shell,
    ViewManagerDelegate* delegate)
    : shell_(shell), delegate_(delegate) {
}

ViewManagerClientFactory::~ViewManagerClientFactory() {
}

// static
scoped_ptr<ViewManagerClient>
ViewManagerClientFactory::WeakBindViewManagerToPipe(
    ScopedMessagePipeHandle handle,
    Shell* shell,
    ViewManagerDelegate* delegate) {
  const bool delete_on_error = false;
  scoped_ptr<ViewManagerClientImpl> client(new ViewManagerClientImpl(
      delegate, shell, handle.Pass(), delete_on_error));
  return client.Pass();
}

// InterfaceFactory<ViewManagerClient> implementation.
void ViewManagerClientFactory::Create(
    ApplicationConnection* connection,
    InterfaceRequest<ViewManagerClient> request) {
  const bool delete_on_error = true;
  new ViewManagerClientImpl(delegate_, shell_, request.PassMessagePipe(),
                            delete_on_error);
}

}  // namespace mojo
