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

namespace content {

class CONTENT_EXPORT ServiceRegistryImpl
    : public ServiceRegistry,
      public NON_EXPORTED_BASE(shell::mojom::InterfaceProvider) {
 public:
  using ServiceFactory = base::Callback<void(mojo::ScopedMessagePipeHandle)>;

  ServiceRegistryImpl();
  ~ServiceRegistryImpl() override;

  // ServiceRegistry overrides.
  void Bind(shell::mojom::InterfaceProviderRequest request) override;
  void BindRemoteServiceProvider(
      shell::mojom::InterfaceProviderPtr service_provider) override;
  void AddService(const std::string& service_name,
                  const ServiceFactory service_factory) override;
  void RemoveService(const std::string& service_name) override;
  void ConnectToRemoteService(const base::StringPiece& service_name,
                              mojo::ScopedMessagePipeHandle handle) override;
  void AddServiceOverrideForTesting(
      const std::string& service_name,
      const ServiceFactory& service_factory) override;
  void ClearServiceOverridesForTesting() override;

  bool IsBound() const;

  base::WeakPtr<ServiceRegistry> GetWeakPtr();

 private:
  // mojo::InterfaceProvider overrides.
  void GetInterface(const mojo::String& name,
                    mojo::ScopedMessagePipeHandle client_handle) override;

  void OnConnectionError();

  mojo::Binding<shell::mojom::InterfaceProvider> binding_;
  shell::mojom::InterfaceProviderPtr remote_provider_;

  std::map<std::string, ServiceFactory> service_factories_;
  std::queue<std::pair<std::string, mojo::MessagePipeHandle> >
      pending_connects_;

  std::map<std::string, ServiceFactory> service_overrides_;

  base::WeakPtrFactory<ServiceRegistry> weak_factory_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_SERVICE_REGISTRY_IMPL_H_
