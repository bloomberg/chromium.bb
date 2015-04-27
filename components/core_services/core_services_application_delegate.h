// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CORE_SERVICES_APPLICATION_DELEGATE_H_
#define COMPONENTS_CORE_SERVICES_APPLICATION_DELEGATE_H_

#include "base/macros.h"
#include "mojo/common/weak_binding_set.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_delegate.h"
#include "third_party/mojo/src/mojo/public/cpp/application/interface_factory_impl.h"
#include "third_party/mojo/src/mojo/public/interfaces/application/service_provider.mojom.h"

namespace core_services {

// The CoreServices application is a singleton ServiceProvider. There is one
// instance of the CoreServices ServiceProvider.
class CoreServicesApplicationDelegate
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::ServiceProvider>,
      public mojo::ServiceProvider {
 public:
  CoreServicesApplicationDelegate();
  ~CoreServicesApplicationDelegate() override;

 private:
  // Overridden from mojo::ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override;

  // Overridden from mojo::InterfaceFactory<ServiceProvider>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<ServiceProvider> request) override;

  // Overridden from ServiceProvider:
  void ConnectToService(const mojo::String& service_name,
                        mojo::ScopedMessagePipeHandle client_handle) override;

  // Bindings for all of our connections.
  mojo::WeakBindingSet<ServiceProvider> provider_bindings_;

  DISALLOW_COPY_AND_ASSIGN(CoreServicesApplicationDelegate);
};

}  // namespace core_services

#endif  // COMPONENTS_CORE_SERVICES_APPLICATION_DELEGATE_H_
