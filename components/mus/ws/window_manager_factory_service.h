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

class WindowManagerFactoryService : public mojom::WindowManagerFactoryService {
 public:
  explicit WindowManagerFactoryService(
      mojo::InterfaceRequest<mojom::WindowManagerFactoryService> request);
  ~WindowManagerFactoryService() override;

  mojom::WindowManagerFactory* window_manager_factory() {
    return window_manager_factory_.get();
  }

  // mojom::WindowManagerFactoryService:
  void SetWindowManagerFactory(mojom::WindowManagerFactoryPtr factory) override;

 private:
  mojo::Binding<mojom::WindowManagerFactoryService> binding_;
  mojom::WindowManagerFactoryPtr window_manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerFactoryService);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_MANAGER_FACTORY_SERVICE_H_
