// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_SERVICE_REGISTRY_IMPL_H_
#define CONTENT_COMMON_MOJO_SERVICE_REGISTRY_IMPL_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/mojo/service_registry_for_route.h"
#include "content/common/routed_service_provider.mojom.h"
#include "content/public/common/service_registry.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {

class CONTENT_EXPORT ServiceRegistryImpl
    : public ServiceRegistry,
      public NON_EXPORTED_BASE(RoutedServiceProvider) {
 public:
  using ServiceFactory = base::Callback<void(mojo::ScopedMessagePipeHandle)>;

  ServiceRegistryImpl();
  ~ServiceRegistryImpl() override;

  // Binds this ServiceProvider implementation to a message pipe endpoint.
  void Bind(mojo::InterfaceRequest<RoutedServiceProvider> request);

  // Binds to a remote ServiceProvider. This will expose added services to the
  // remote ServiceProvider with the corresponding handle and enable
  // ConnectToRemoteService to provide access to services exposed by the remote
  // ServiceProvider.
  void BindRemoteServiceProvider(RoutedServiceProviderPtr service_provider);

  // Creates a new ServiceRegistryForRoute bound to this underlying registry
  // and the given |route_id|.
  scoped_ptr<ServiceRegistryForRoute> CreateServiceRegistryForRoute(
      int route_id);

  // ServiceRegistry overrides.
  void Add(const std::string& service_name,
           const base::Callback<void(mojo::ScopedMessagePipeHandle)>
           service_factory) override;
  void Remove(const std::string& service_name) override;
  void Connect(const base::StringPiece& service_name,
               mojo::ScopedMessagePipeHandle handle) override;

 private:
  friend class ServiceRegistryForRoute;
  class ConnectionRequest;

  using ServiceFactoryMap =
      std::map<std::string,
               base::Callback<void(mojo::ScopedMessagePipeHandle)>>;
  using ConnectionRequests = std::vector<scoped_ptr<ConnectionRequest>>;
  using RoutedConnectionRequestMap = std::map<int, ConnectionRequests>;

  // RoutedServiceProvider overrides.
  void ConnectToService(const mojo::String& name,
                        mojo::ScopedMessagePipeHandle pipe) override;
  void ConnectToServiceRouted(int32_t route_id,
                              const mojo::String& name,
                              mojo::ScopedMessagePipeHandle pipe) override;

  // Methods used by ServiceRegistryForRoute.
  void AddRouted(int route_id,
                 const std::string& service_name,
                 const ServiceFactory& service_factory);
  void RemoveRouted(int route_id, const std::string& service_name);
  void ConnectRouted(int route_id,
                     const std::string& service_name,
                     mojo::ScopedMessagePipeHandle handle);

  // Closes a route, rejecting any pending connections to or from its route ID
  // and dropping any service factories attached to it.
  void CloseRoute(int route_id);

  mojo::Binding<RoutedServiceProvider> binding_;
  RoutedServiceProviderPtr remote_provider_;

  ServiceFactoryMap service_factories_;
  std::map<int, ServiceFactoryMap> routed_service_factories_;

  // A queue of outgoing connection requests which will accumulate as long
  // as the remote ServiceProvider is not connected.
  ConnectionRequests pending_outgoing_connects_;

  // Queues of routed connection requests. Outgoing requests will accumulate
  // as long as the remote ServiceProvider is not connection. Incoming requests
  // will accumulate until the route is established via a call to
  // CreateServiceRegistryForRoute.
  RoutedConnectionRequestMap pending_routed_incoming_connects_;
  RoutedConnectionRequestMap pending_routed_outgoing_connects_;

  base::WeakPtrFactory<ServiceRegistryImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceRegistryImpl);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SERVICE_REGISTRY_IMPL_H_
