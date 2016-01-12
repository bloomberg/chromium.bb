// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_SERVICE_REGISTRY_IMPL_H_
#define CONTENT_COMMON_MOJO_SERVICE_REGISTRY_IMPL_H_

#include <map>
#include <queue>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/common/service_registry.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/public/interfaces/service_provider.mojom.h"

namespace content {

class CONTENT_EXPORT ServiceRegistryImpl
    : public ServiceRegistry,
      public NON_EXPORTED_BASE(mojo::ServiceProvider) {
 public:
  ServiceRegistryImpl();
  ~ServiceRegistryImpl() override;

  // Binds this ServiceProvider implementation to a message pipe endpoint.
  void Bind(mojo::InterfaceRequest<mojo::ServiceProvider> request);

  // Binds to a remote ServiceProvider. This will expose added services to the
  // remote ServiceProvider with the corresponding handle and enable
  // ConnectToRemoteService to provide access to services exposed by the remote
  // ServiceProvider.
  void BindRemoteServiceProvider(mojo::ServiceProviderPtr service_provider);

  // ServiceRegistry overrides.
  void AddService(const std::string& service_name,
                  const base::Callback<void(mojo::ScopedMessagePipeHandle)>
                      service_factory) override;
  void RemoveService(const std::string& service_name) override;
  void ConnectToRemoteService(const base::StringPiece& service_name,
                              mojo::ScopedMessagePipeHandle handle) override;

  bool IsBound() const;

  base::WeakPtr<ServiceRegistry> GetWeakPtr();

 private:
  // mojo::ServiceProvider overrides.
  void ConnectToService(const mojo::String& name,
                        mojo::ScopedMessagePipeHandle client_handle) override;

  void OnConnectionError();

  mojo::Binding<mojo::ServiceProvider> binding_;
  mojo::ServiceProviderPtr remote_provider_;

  std::map<std::string, base::Callback<void(mojo::ScopedMessagePipeHandle)> >
      service_factories_;
  std::queue<std::pair<std::string, mojo::MessagePipeHandle> >
      pending_connects_;

  base::WeakPtrFactory<ServiceRegistry> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SERVICE_REGISTRY_IMPL_H_
