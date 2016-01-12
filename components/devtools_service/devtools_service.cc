// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_service.h"

#include <utility>

#include "base/logging.h"
#include "components/devtools_service/devtools_http_server.h"
#include "mojo/shell/public/cpp/application_impl.h"

namespace devtools_service {

DevToolsService::DevToolsService(mojo::ApplicationImpl* application)
    : application_(application), registry_(this) {
  DCHECK(application_);
}

DevToolsService::~DevToolsService() {
}

void DevToolsService::BindToCoordinatorRequest(
    mojo::InterfaceRequest<DevToolsCoordinator> request) {
  coordinator_bindings_.AddBinding(this, std::move(request));
}

void DevToolsService::Initialize(uint16_t remote_debugging_port) {
  if (http_server_) {
    LOG(WARNING) << "DevTools service receives a "
                 << "DevToolsCoordinator.Initialize() call while it has "
                 << "already been initialized.";
    return;
  }

  http_server_.reset(new DevToolsHttpServer(this, remote_debugging_port));
}

}  // namespace devtools_service
