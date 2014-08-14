// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/application/service_provider_impl.h"

#include "mojo/public/cpp/application/lib/service_connector.h"
#include "mojo/public/cpp/application/lib/weak_service_provider.h"
#include "mojo/public/cpp/environment/logging.h"

namespace mojo {

ServiceProviderImpl::ServiceProviderImpl() : remote_(NULL) {
}

ServiceProviderImpl::~ServiceProviderImpl() {
}

ServiceProvider* ServiceProviderImpl::CreateRemoteServiceProvider() {
  // TODO(beng): it sure would be nice if this method could return a scoped_ptr.
  MOJO_DCHECK(!remote_);
  remote_ = new internal::WeakServiceProvider(this, client());
  return remote_;
}

void ServiceProviderImpl::ConnectToService(
    const String& service_name,
    ScopedMessagePipeHandle client_handle) {
  if (service_connectors_.find(service_name) == service_connectors_.end()) {
    client_handle.reset();
    return;
  }

  internal::ServiceConnectorBase* service_connector =
      service_connectors_[service_name];
  return service_connector->ConnectToService(service_name,
                                             client_handle.Pass());
}

void ServiceProviderImpl::OnConnectionError() {
  ClearRemote();
}

void ServiceProviderImpl::AddServiceConnector(
    internal::ServiceConnectorBase* service_connector) {
  RemoveServiceConnector(service_connector);
  service_connectors_[service_connector->name()] = service_connector;
  // TODO(beng): perhaps take app connection thru ctor??
  service_connector->set_application_connection(NULL);
}

void ServiceProviderImpl::RemoveServiceConnector(
    internal::ServiceConnectorBase* service_connector) {
  NameToServiceConnectorMap::iterator it =
      service_connectors_.find(service_connector->name());
  if (it == service_connectors_.end())
    return;
  delete it->second;
  service_connectors_.erase(it);
}

void ServiceProviderImpl::ClearRemote() {
  if (remote_) {
    remote_->Clear();
    remote_ = NULL;
  }
}

}  // namespace mojo
