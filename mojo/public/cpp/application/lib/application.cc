// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/application.h"

namespace mojo {

Application::Application() {}

Application::Application(ScopedMessagePipeHandle service_provider_handle)
    : internal::ServiceConnectorBase::Owner(service_provider_handle.Pass()) {
}

Application::Application(MojoHandle service_provider_handle)
    : internal::ServiceConnectorBase::Owner(
          mojo::MakeScopedHandle(
              MessagePipeHandle(service_provider_handle)).Pass()) {}

Application::~Application() {
  for (ServiceConnectorList::iterator it = service_connectors_.begin();
       it != service_connectors_.end(); ++it) {
    delete *it;
  }
}

void Application::Initialize() {}

void Application::AddServiceConnector(
    internal::ServiceConnectorBase* service_connector) {
  service_connectors_.push_back(service_connector);
  set_service_connector_owner(service_connector, this);
}

void Application::RemoveServiceConnector(
    internal::ServiceConnectorBase* service_connector) {
  for (ServiceConnectorList::iterator it = service_connectors_.begin();
       it != service_connectors_.end(); ++it) {
    if (*it == service_connector) {
      service_connectors_.erase(it);
      delete service_connector;
      break;
    }
  }
  if (service_connectors_.empty())
    service_provider_.reset();
}

void Application::BindServiceProvider(
    ScopedMessagePipeHandle service_provider_handle) {
  service_provider_.Bind(service_provider_handle.Pass());
  service_provider_.set_client(this);
}

void Application::ConnectToService(const mojo::String& url,
                                   ScopedMessagePipeHandle client_handle) {
  // TODO(davemoore): This method must be overridden by an Application subclass
  // to dispatch to the right ServiceConnector. We need to figure out an
  // approach to registration to make this better.
  assert(1 == service_connectors_.size());
  return service_connectors_.front()->ConnectToService(url.To<std::string>(),
                                                       client_handle.Pass());
}

}  // namespace mojo
