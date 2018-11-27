// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_MULTI_USER_WINDOW_MANAGER_BRIDGE_H_
#define ASH_WS_MULTI_USER_WINDOW_MANAGER_BRIDGE_H_

#include "ash/multi_user/multi_user_window_manager_window_delegate.h"
#include "ash/public/interfaces/multi_user_window_manager.mojom.h"
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

// Trivially forwards calls to MultiUserWindowManager.
class MultiUserWindowManagerBridge
    : public mojom::MultiUserWindowManager,
      public ws::WindowManagerInterface,
      public MultiUserWindowManagerWindowDelegate {
 public:
  MultiUserWindowManagerBridge(ws::WindowTree* window_tree,
                               mojo::ScopedInterfaceEndpointHandle handle);
  ~MultiUserWindowManagerBridge() override;

  // mojom::MultiUserWindowManager overrides:
  void SetClient(mojom::MultiUserWindowManagerClientAssociatedPtrInfo
                     client_info) override;
  void SetWindowOwner(ws::Id window_id,
                      const AccountId& account_id,
                      bool show_for_current_user) override;
  void ShowWindowForUser(ws::Id window_id,
                         const AccountId& account_id) override;

 private:
  // MultiUserWindowManagerWindowDelegate:
  void OnWindowOwnerEntryChanged(aura::Window* window,
                                 const AccountId& account_id,
                                 bool was_minimized,
                                 bool teleported) override;

  ws::WindowTree* window_tree_;
  mojo::AssociatedBinding<mojom::MultiUserWindowManager> binding_;
  mojom::MultiUserWindowManagerClientAssociatedPtr client_;

  DISALLOW_COPY_AND_ASSIGN(MultiUserWindowManagerBridge);
};

}  // namespace ash

#endif  // ASH_WS_MULTI_USER_WINDOW_MANAGER_BRIDGE_H_
