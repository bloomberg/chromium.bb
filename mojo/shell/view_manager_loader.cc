// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/view_manager_loader.h"

#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/view_manager/view_manager_init_service_impl.h"

namespace mojo {

using service::ViewManagerInitServiceImpl;

namespace shell {

ViewManagerLoader::ViewManagerLoader() {
}

ViewManagerLoader::~ViewManagerLoader() {
}

void ViewManagerLoader::Load(ApplicationManager* manager,
                             const GURL& url,
                             scoped_refptr<LoadCallbacks> callbacks) {
  ScopedMessagePipeHandle shell_handle = callbacks->RegisterApplication();
  if (!shell_handle.is_valid())
    return;

  // TODO(sky): this needs some sort of authentication as well as making sure
  // we only ever have one active at a time.
  scoped_ptr<ApplicationImpl> app(
      new ApplicationImpl(this, shell_handle.Pass()));
  apps_.push_back(app.release());
}

void ViewManagerLoader::OnApplicationError(ApplicationManager* manager,
                                           const GURL& url) {
}

bool ViewManagerLoader::ConfigureIncomingConnection(
    ApplicationConnection* connection)  {
  context_.ConfigureIncomingConnection(connection);
  connection->AddService(this);
  return true;
}

void ViewManagerLoader::Create(
    ApplicationConnection* connection,
    InterfaceRequest<ViewManagerInitService> request) {
  BindToRequest(new ViewManagerInitServiceImpl(connection, &context_),
                &request);
}

}  // namespace shell
}  // namespace mojo
