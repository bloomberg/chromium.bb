// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/multi_user_window_manager_bridge.h"

#include "ash/multi_user/multi_user_window_manager.h"
#include "services/ws/window_tree.h"
#include "ui/aura/window.h"

namespace ash {

MultiUserWindowManagerBridge::MultiUserWindowManagerBridge(
    ws::WindowTree* window_tree,
    mojo::ScopedInterfaceEndpointHandle handle)
    : window_tree_(window_tree),
      binding_(this,
               mojo::AssociatedInterfaceRequest<mojom::MultiUserWindowManager>(
                   std::move(handle))) {}

MultiUserWindowManagerBridge::~MultiUserWindowManagerBridge() {
  // We may get here after MultiUserWindowManager has been destroyed.
  if (ash::MultiUserWindowManager::Get())
    ash::MultiUserWindowManager::Get()->RemoveWindowDelegate(this);
}

void MultiUserWindowManagerBridge::SetClient(
    mojom::MultiUserWindowManagerClientAssociatedPtrInfo client_info) {
  client_.Bind(std::move(client_info));
}

void MultiUserWindowManagerBridge::SetWindowOwner(ws::Id window_id,
                                                  const AccountId& account_id,
                                                  bool show_for_current_user) {
  // At this time this is only called once MultiUserWindowManager has been
  // created. This needs to be fixed for the multi-process case.
  // http://crbug.com/875111.
  DCHECK(ash::MultiUserWindowManager::Get());
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window && window_tree_->IsTopLevel(window)) {
    ash::MultiUserWindowManager::Get()->SetWindowOwner(
        window, account_id, show_for_current_user, this);
  } else {
    DVLOG(1) << "SetWindowOwner passed invalid window, id=" << window_id;
  }
}

void MultiUserWindowManagerBridge::ShowWindowForUser(
    ws::Id window_id,
    const AccountId& account_id) {
  // At this time this is only called once MultiUserWindowManager has been
  // created. This needs to be fixed for the multi-process case.
  // http://crbug.com/875111.
  DCHECK(ash::MultiUserWindowManager::Get());
  aura::Window* window = window_tree_->GetWindowByTransportId(window_id);
  if (window && window_tree_->IsTopLevel(window))
    ash::MultiUserWindowManager::Get()->ShowWindowForUser(window, account_id);
  else
    DVLOG(1) << "ShowWindowForUser passed invalid window, id=" << window_id;
}

void MultiUserWindowManagerBridge::OnWindowOwnerEntryChanged(
    aura::Window* window,
    const AccountId& account_id,
    bool was_minimized,
    bool teleported) {
  if (!client_)
    return;

  client_->OnWindowOwnerEntryChanged(window_tree_->TransportIdForWindow(window),
                                     account_id, was_minimized, teleported);
}

}  // namespace ash
