// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_APPLICATION_SERVICE_PROVIDER_IMPL_H_
#define MOJO_PUBLIC_APPLICATION_SERVICE_PROVIDER_IMPL_H_

#include "mojo/public/cpp/application/lib/service_connector.h"
#include "mojo/public/interfaces/application/service_provider.mojom.h"

namespace mojo {
namespace internal {
class WeakServiceProvider;
class ServiceConnectorBase;
}

// Implements a registry that can be used to expose services to another app.
class ServiceProviderImpl : public InterfaceImpl<ServiceProvider> {
 public:
  ServiceProviderImpl();
  virtual ~ServiceProviderImpl();

  template <typename Interface>
  void AddService(InterfaceFactory<Interface>* factory) {
    AddServiceConnector(
        new internal::InterfaceFactoryConnector<Interface>(factory));
  }

  // Returns an instance of a ServiceProvider that weakly wraps this impl's
  // connection to some other app. Whereas the lifetime of an instance of
  // ServiceProviderImpl is bound to the lifetime of the pipe it
  // encapsulates, the lifetime of the ServiceProvider instance returned by this
  // method is assumed by the caller. After the pipe is closed
  // ConnectToService() calls on this object will be silently dropped.
  // This method must only be called once per ServiceProviderImpl.
  ServiceProvider* CreateRemoteServiceProvider();

 private:
  typedef std::map<std::string, internal::ServiceConnectorBase*>
      NameToServiceConnectorMap;

  friend class internal::WeakServiceProvider;

  // Overridden from ServiceProvider:
  virtual void ConnectToService(
      const String& service_name,
      ScopedMessagePipeHandle client_handle) MOJO_OVERRIDE;

  // Overridden from InterfaceImpl:
  virtual void OnConnectionError() MOJO_OVERRIDE;

  void AddServiceConnector(
      internal::ServiceConnectorBase* service_connector);
  void RemoveServiceConnector(
      internal::ServiceConnectorBase* service_connector);

  void ClearRemote();

  NameToServiceConnectorMap service_connectors_;

  internal::WeakServiceProvider* remote_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ServiceProviderImpl);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_APPLICATION_SERVICE_PROVIDER_IMPL_H_
