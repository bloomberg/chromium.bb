// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_REGISTRY_IMPL_H_
#define COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_REGISTRY_IMPL_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/common/weak_binding_set.h"

namespace devtools_service {

class DevToolsAgentHost;
class DevToolsService;

class DevToolsRegistryImpl : public DevToolsRegistry {
 public:
  class Iterator {
   public:
    // |registry| must outlive this object.
    explicit Iterator(DevToolsRegistryImpl* registry);
    ~Iterator();

    bool IsAtEnd() const { return iter_ == registry_->agents_.end(); }
    void Advance() { ++iter_; }

    DevToolsAgentHost* value() { return iter_->second.get(); }

   private:
    DevToolsRegistryImpl* const registry_;
    std::map<std::string, linked_ptr<DevToolsAgentHost>>::const_iterator iter_;
  };

  // |service| must outlive this object.
  explicit DevToolsRegistryImpl(DevToolsService* service);
  ~DevToolsRegistryImpl() override;

  void BindToRegistryRequest(mojo::InterfaceRequest<DevToolsRegistry> request);

  DevToolsAgentHost* GetAgentById(const std::string& id);

 private:
  // DevToolsRegistry implementation.
  void RegisterAgent(const mojo::String& id, DevToolsAgentPtr agent) override;

  void OnAgentConnectionError(const std::string& id);

  DevToolsService* const service_;

  mojo::WeakBindingSet<DevToolsRegistry> bindings_;

  std::map<std::string, linked_ptr<DevToolsAgentHost>> agents_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsRegistryImpl);
};

}  // namespace devtools_service

#endif  // COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_REGISTRY_IMPL_H_
