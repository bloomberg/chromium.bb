// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/multi_user_window_manager_bridge.h"

#include "ash/multi_user/multi_user_window_manager.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "services/ws/window_tree.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"

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
    ash::MultiUserWindowManager::Get()->SetClient(nullptr);
}

void MultiUserWindowManagerBridge::SetClient(
    mojom::MultiUserWindowManagerClientAssociatedPtrInfo client_info) {
  multi_user_window_manager_.reset();
  client_.Bind(std::move(client_info));
  if (features::IsMultiProcessMash()) {
    // NOTE: there is nothing stopping mulitple MultiUserWindowManagerBridges
    // from being created (because multiple clients ask for
    // ash::mojom::MultiUserWindowManager). This code is assuming only a single
    // client is used at a time.
    multi_user_window_manager_ = std::make_unique<ash::MultiUserWindowManager>(
        client_.get(), nullptr,
        Shell::Get()->session_controller()->GetActiveAccountId());
  } else {
    ash::MultiUserWindowManager::Get()->SetClient(client_.get());
  }
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
        window, account_id, show_for_current_user, {window_id});
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

}  // namespace ash
