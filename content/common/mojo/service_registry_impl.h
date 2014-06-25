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
#include "content/public/common/service_registry.h"
#include "mojo/public/cpp/bindings/interface_impl.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/interface_provider/interface_provider.mojom.h"

namespace content {

class ServiceRegistryImpl
    : public ServiceRegistry,
      public mojo::InterfaceImpl<mojo::IInterfaceProvider> {
 public:
  ServiceRegistryImpl();
  explicit ServiceRegistryImpl(mojo::ScopedMessagePipeHandle handle);
  virtual ~ServiceRegistryImpl();

  // Binds to a remote ServiceProvider. This will expose added services to the
  // remote ServiceProvider with the corresponding handle and enable
  // GetInterface to provide access to services exposed by the remote
  // ServiceProvider.
  void BindRemoteServiceProvider(mojo::ScopedMessagePipeHandle handle);

  // ServiceRegistry overrides.
  virtual void AddService(
      const std::string& service_name,
      const base::Callback<void(mojo::ScopedMessagePipeHandle)> service_factory)
      OVERRIDE;
  virtual void RemoveService(const std::string& service_name) OVERRIDE;
  virtual void GetRemoteInterface(
      const base::StringPiece& service_name,
      mojo::ScopedMessagePipeHandle handle) OVERRIDE;

 private:
  // mojo::InterfaceImpl<mojo::IInterfaceProvider> overrides.
  virtual void GetInterface(
      const mojo::String& name,
      mojo::ScopedMessagePipeHandle client_handle) OVERRIDE;
  virtual void OnConnectionError() OVERRIDE;

  std::map<std::string, base::Callback<void(mojo::ScopedMessagePipeHandle)> >
      service_factories_;
  std::queue<std::pair<std::string, mojo::MessagePipeHandle> >
      pending_connects_;
  bool bound_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SERVICE_REGISTRY_IMPL_H_
