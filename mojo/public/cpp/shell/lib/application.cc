// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/shell/application.h"

namespace mojo {

Application::Application(ScopedShellHandle shell_handle)
    : internal::ServiceConnectorBase::Owner(shell_handle.Pass()) {
}

Application::Application(MojoHandle shell_handle)
    : internal::ServiceConnectorBase::Owner(
          mojo::MakeScopedHandle(ShellHandle(shell_handle)).Pass()) {}

Application::~Application() {
  for (ServiceConnectorList::iterator it = service_connectors_.begin();
       it != service_connectors_.end(); ++it) {
    delete *it;
  }
}

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
    shell_.reset();
}

void Application::AcceptConnection(const mojo::String& url,
                                   ScopedMessagePipeHandle client_handle) {
  // TODO(davemoore): This method must be overridden by an Application subclass
  // to dispatch to the right ServiceConnector. We need to figure out an
  // approach to registration to make this better.
  assert(1 == service_connectors_.size());
  return service_connectors_.front()->AcceptConnection(url.To<std::string>(),
                                                       client_handle.Pass());
}

}  // namespace mojo
