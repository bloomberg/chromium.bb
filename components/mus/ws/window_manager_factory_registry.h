// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/window_manager_factory.mojom.h"

namespace mus {
namespace ws {

class ConnectionManager;
class WindowManagerFactoryService;

// WindowManagerFactoryRegistry tracks the set of registered
// WindowManagerFactoryServices.
class WindowManagerFactoryRegistry {
 public:
  explicit WindowManagerFactoryRegistry(ConnectionManager* connection_manager);
  ~WindowManagerFactoryRegistry();

  void Register(
      uint32_t user_id,
      mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request);

  std::vector<WindowManagerFactoryService*> GetServices();

 private:
  friend class WindowManagerFactoryService;

  void OnWindowManagerFactoryConnectionLost(
      WindowManagerFactoryService* service);
  void OnWindowManagerFactorySet();

  ConnectionManager* connection_manager_;

  std::vector<scoped_ptr<WindowManagerFactoryService>> services_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerFactoryRegistry);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_REGISTRY_H_
