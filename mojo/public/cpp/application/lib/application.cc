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
  for (NameToServiceConnectorMap::iterator i =
           name_to_service_connector_.begin();
       i != name_to_service_connector_.end(); ++i) {
    delete i->second;
  }
  name_to_service_connector_.clear();
}

void Application::Initialize() {}

void Application::AddServiceConnector(
    internal::ServiceConnectorBase* service_connector) {
  name_to_service_connector_[service_connector->name()] = service_connector;
  set_service_connector_owner(service_connector, this);
}

void Application::RemoveServiceConnector(
    internal::ServiceConnectorBase* service_connector) {
  NameToServiceConnectorMap::iterator it =
      name_to_service_connector_.find(service_connector->name());
  assert(it != name_to_service_connector_.end());
  delete it->second;
  name_to_service_connector_.erase(it);
  if (name_to_service_connector_.empty())
    service_provider_.reset();
}

void Application::BindServiceProvider(
    ScopedMessagePipeHandle service_provider_handle) {
  service_provider_.Bind(service_provider_handle.Pass());
  service_provider_.set_client(this);
}

void Application::ConnectToService(const mojo::String& service_url,
                                   const mojo::String& service_name,
                                   ScopedMessagePipeHandle client_handle,
                                   const mojo::String& requestor_url) {
  internal::ServiceConnectorBase* service_connector =
      name_to_service_connector_[service_name];
  assert(service_connector);
  // requestor_url is ignored because the service_connector stores the url
  // of the requestor safely.
  return service_connector->ConnectToService(
      service_url, service_name, client_handle.Pass());
}

}  // namespace mojo
