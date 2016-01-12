// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_service_delegate.h"

#include <utility>

#include "base/logging.h"
#include "components/devtools_service/devtools_registry_impl.h"
#include "components/devtools_service/devtools_service.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "url/gurl.h"

namespace devtools_service {

namespace {

bool IsShell(const GURL& requestor_url) {
  // TODO(yzshen): http://crbug.com/491656 "mojo://shell/" has to be used
  // instead of "mojo:shell" because "mojo" is not treated as a standard scheme.
  return requestor_url == GURL("mojo://shell/");
}

}  // namespace

DevToolsServiceDelegate::DevToolsServiceDelegate() {
}

DevToolsServiceDelegate::~DevToolsServiceDelegate() {
}

void DevToolsServiceDelegate::Initialize(mojo::ApplicationImpl* app) {
  service_.reset(new DevToolsService(app));
}

bool DevToolsServiceDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService<DevToolsRegistry>(this);

  // DevToolsCoordinator is a privileged interface and only allowed for the
  // shell.
  if (IsShell(GURL(connection->GetRemoteApplicationURL())))
    connection->AddService<DevToolsCoordinator>(this);
  return true;
}

void DevToolsServiceDelegate::Quit() {
  service_.reset();
}

void DevToolsServiceDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<DevToolsRegistry> request) {
  service_->registry()->BindToRegistryRequest(std::move(request));
}

void DevToolsServiceDelegate::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<DevToolsCoordinator> request) {
  service_->BindToCoordinatorRequest(std::move(request));
}

}  // namespace devtools_service
