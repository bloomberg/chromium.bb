// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_REGISTRY_IMPL_H_
#define COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_REGISTRY_IMPL_H_

#include "base/macros.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/common/weak_binding_set.h"

namespace devtools_service {

class DevToolsService;

class DevToolsRegistryImpl : public DevToolsRegistry {
 public:
  // |service| must outlive this object.
  explicit DevToolsRegistryImpl(DevToolsService* service);
  ~DevToolsRegistryImpl() override;

  void BindToRegistryRequest(mojo::InterfaceRequest<DevToolsRegistry> request);

 private:
  // DevToolsRegistry implementation.
  void RegisterAgent(DevToolsAgentPtr agent) override;

  DevToolsService* const service_;

  mojo::WeakBindingSet<DevToolsRegistry> bindings_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsRegistryImpl);
};

}  // namespace devtools_service

#endif  // COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_REGISTRY_IMPL_H_
