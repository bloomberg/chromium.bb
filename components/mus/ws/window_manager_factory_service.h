// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_SERVICE_H_
#define COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_SERVICE_H_

#include <stdint.h>

#include "components/mus/public/interfaces/window_manager_factory.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace mus {
namespace ws {

class ServerWindow;
class WindowManagerFactoryRegistry;

class WindowManagerFactoryService : public mojom::WindowManagerFactoryService {
 public:
  WindowManagerFactoryService(
      WindowManagerFactoryRegistry* registry,
      uint32_t user_id,
      mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request);
  ~WindowManagerFactoryService() override;

  uint32_t user_id() const { return user_id_; }

  mojom::WindowManagerFactory* window_manager_factory() {
    return window_manager_factory_.get();
  }

  // mojom::WindowManagerFactoryService:
  void SetWindowManagerFactory(mojom::WindowManagerFactoryPtr factory) override;

 private:
  void OnConnectionLost();

  WindowManagerFactoryRegistry* registry_;
  const uint32_t user_id_;
  mojo::Binding<mojom::WindowManagerFactoryService> binding_;
  mojom::WindowManagerFactoryPtr window_manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerFactoryService);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_SERVICE_H_
