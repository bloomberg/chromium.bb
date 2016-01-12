// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_SERVICE_PROVIDER_IMPL_H_
#define MOJO_SHELL_PUBLIC_CPP_SERVICE_PROVIDER_IMPL_H_

#include <string>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/public/cpp/lib/interface_factory_connector.h"
#include "mojo/shell/public/cpp/lib/service_connector_registry.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"

namespace mojo {

// Implements a registry that can be used to expose services to another app.
class ServiceProviderImpl : public ServiceProvider {
 public:
  ServiceProviderImpl();
  explicit ServiceProviderImpl(InterfaceRequest<ServiceProvider> request);
  ~ServiceProviderImpl() override;

  void Bind(InterfaceRequest<ServiceProvider> request);

  template <typename Interface>
  void AddService(InterfaceFactory<Interface>* factory) {
    SetServiceConnectorForName(
        new internal::InterfaceFactoryConnector<Interface>(factory),
        Interface::Name_);
  }

 private:
  // Overridden from ServiceProvider:
  void ConnectToService(const String& service_name,
                        ScopedMessagePipeHandle client_handle) override;

  void SetServiceConnectorForName(ServiceConnector* service_connector,
                                  const std::string& interface_name);

  StrongBinding<ServiceProvider> binding_;

  internal::ServiceConnectorRegistry service_connector_registry_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceProviderImpl);
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SERVICE_PROVIDER_IMPL_H_
