// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/window_service_delegate_impl.h"

#include "ash/wm/container_finder.h"
#include "ui/aura/window.h"

namespace ash {

WindowServiceDelegateImpl::WindowServiceDelegateImpl() = default;

WindowServiceDelegateImpl::~WindowServiceDelegateImpl() = default;

std::unique_ptr<aura::Window> WindowServiceDelegateImpl::NewTopLevel(
    const base::flat_map<std::string, std::vector<uint8_t>>& properties) {
  // TODO: this needs to call CreateAndParentTopLevelWindow();
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_NOT_DRAWN);
  ash::wm::GetDefaultParent(window.get(), gfx::Rect())->AddChild(window.get());
  return window;
}

}  // namespace ash
