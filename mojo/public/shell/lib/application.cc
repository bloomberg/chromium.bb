// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/shell/application.h"

namespace mojo {

Application::Application(ScopedShellHandle shell_handle)
    : shell_(shell_handle.Pass(), this) {
}

Application::Application(MojoHandle shell_handle)
    : shell_(mojo::MakeScopedHandle(ShellHandle(shell_handle)).Pass()) {}

Application::~Application() {
  for (ServiceFactoryList::iterator it = service_factories_.begin();
       it != service_factories_.end(); ++it) {
    delete *it;
  }
}

Shell* Application::GetShell() {
  return shell_.get();
}

void Application::AddServiceFactory(
    internal::ServiceFactoryBase* service_factory) {
  service_factories_.push_back(service_factory);
  set_service_factory_owner(service_factory, this);
}

void Application::RemoveServiceFactory(
    internal::ServiceFactoryBase* service_factory) {
  for (ServiceFactoryList::iterator it = service_factories_.begin();
       it != service_factories_.end(); ++it) {
    if (*it == service_factory) {
      service_factories_.erase(it);
      delete service_factory;
      break;
    }
  }
  if (service_factories_.empty())
    shell_.reset();
}

void Application::AcceptConnection(const mojo::String& url,
                                   ScopedMessagePipeHandle client_handle) {
  // TODO(davemoore): This method must be overridden by an Application subclass
  // to dispatch to the right ServiceFactory. We need to figure out an approach
  // to registration to make this better.
  assert(1 == service_factories_.size());
  return service_factories_.front()->AcceptConnection(url.To<std::string>(),
                                                      client_handle.Pass());
}

}  // namespace mojo
