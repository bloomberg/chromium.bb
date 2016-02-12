// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_WS_WINDOW_TREE_FACTORY_H_
#define COMPONENTS_MUS_WS_WINDOW_TREE_FACTORY_H_

#include "base/macros.h"
#include "components/mus/public/interfaces/window_tree.mojom.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"

namespace mus {
namespace ws {

class ConnectionManager;

class WindowTreeFactory : public mus::mojom::WindowTreeFactory {
 public:
  explicit WindowTreeFactory(ConnectionManager* connection_manager);
  ~WindowTreeFactory() override;

  void AddBinding(
      mojo::InterfaceRequest<mus::mojom::WindowTreeFactory> request);

  // mus::mojom::WindowTreeFactory:
  void CreateWindowTree(mojo::InterfaceRequest<mojom::WindowTree> tree_request,
                        mojom::WindowTreeClientPtr client) override;

 private:
  ConnectionManager* connection_manager_;

  mojo::WeakBindingSet<mus::mojom::WindowTreeFactory> binding_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeFactory);
};

}  // namespace ws
}  // namespace mus

#endif  // COMPONENTS_MUS_WS_WINDOW_TREE_FACTORY_H_
