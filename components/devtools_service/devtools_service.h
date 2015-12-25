// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_H_
#define COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/devtools_service/devtools_registry_impl.h"
#include "components/devtools_service/public/interfaces/devtools_service.mojom.h"
#include "mojo/common/weak_binding_set.h"

namespace mojo {
class ApplicationImpl;
}

namespace devtools_service {

class DevToolsHttpServer;

// DevToolsService is the central control. It manages the communication with
// DevTools agents (e.g., Web page renderers). It also starts an HTTP server to
// speak the Chrome remote debugging protocol.
class DevToolsService : public DevToolsCoordinator {
 public:
  // Doesn't take ownership of |application|, which must outlive this object.
  explicit DevToolsService(mojo::ApplicationImpl* application);
  ~DevToolsService() override;

  void BindToCoordinatorRequest(
      mojo::InterfaceRequest<DevToolsCoordinator> request);

  mojo::ApplicationImpl* application() { return application_; }

  DevToolsRegistryImpl* registry() { return &registry_; }

 private:
  // DevToolsCoordinator implementation.
  void Initialize(uint16_t remote_debugging_port) override;

  // Not owned by this object.
  mojo::ApplicationImpl* const application_;

  mojo::WeakBindingSet<DevToolsCoordinator> coordinator_bindings_;

  scoped_ptr<DevToolsHttpServer> http_server_;
  DevToolsRegistryImpl registry_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsService);
};

}  // namespace devtools_service

#endif  // COMPONENTS_DEVTOOLS_SERVICE_DEVTOOLS_SERVICE_H_
