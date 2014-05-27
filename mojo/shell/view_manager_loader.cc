// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/view_manager_loader.h"

#include "mojo/public/cpp/application/application.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/view_manager_connection.h"

namespace mojo {
namespace shell {

ViewManagerLoader::ViewManagerLoader() {
}

ViewManagerLoader::~ViewManagerLoader() {
}

void ViewManagerLoader::LoadService(
    ServiceManager* manager,
    const GURL& url,
    ScopedMessagePipeHandle service_provider_handle) {
  scoped_ptr<Application> app(new Application(service_provider_handle.Pass()));
  if (!root_node_manager_.get()) {
    root_node_manager_.reset(
        new view_manager::service::RootNodeManager(app->service_provider()));
  }
  app->AddService<view_manager::service::ViewManagerConnection>(
      root_node_manager_.get());
  apps_.push_back(app.release());
}

void ViewManagerLoader::OnServiceError(ServiceManager* manager,
                                       const GURL& url) {
}

}  // namespace shell
}  // namespace mojo
