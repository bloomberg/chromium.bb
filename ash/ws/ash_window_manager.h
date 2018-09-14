// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_ASH_WINDOW_MANAGER_H_
#define ASH_WS_ASH_WINDOW_MANAGER_H_

#include "ash/public/interfaces/ash_window_manager.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "services/ws/common/types.h"
#include "services/ws/window_manager_interface.h"

namespace mojo {
class ScopedInterfaceEndpointHandle;
}

namespace ws {
class WindowTree;
}

namespace ash {

// Implementation of mojom::AshWindowManager, see it for details.
class AshWindowManager : public mojom::AshWindowManager,
                         public ws::WindowManagerInterface {
 public:
  AshWindowManager(ws::WindowTree* window_tree,
                   mojo::ScopedInterfaceEndpointHandle handle);
  ~AshWindowManager() override;

  // mojom::AshWindowManager:
  void AddWindowToTabletMode(ws::Id window_id) override;

 private:
  ws::WindowTree* window_tree_;
  mojo::AssociatedBinding<mojom::AshWindowManager> binding_;

  DISALLOW_COPY_AND_ASSIGN(AshWindowManager);
};

}  // namespace ash

#endif  // ASH_WS_ASH_WINDOW_MANAGER_H_
