// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/virtual_keyboard_container_layout_manager.h"

#include "ash/shell_window_ids.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_controller.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// VirtualKeyboardContainerLayoutManager, public:

VirtualKeyboardContainerLayoutManager::VirtualKeyboardContainerLayoutManager(
    aura::Window* container)
    : SnapToPixelLayoutManager(container),
      parent_container_(container) {}

VirtualKeyboardContainerLayoutManager::~VirtualKeyboardContainerLayoutManager()
{
}

////////////////////////////////////////////////////////////////////////////////
// VirtualKeyboardContainerLayoutManager, aura::LayoutManager implementation:

void VirtualKeyboardContainerLayoutManager::OnWindowResized() {
  keyboard::KeyboardController* keyboard_controller =
      keyboard::KeyboardController::GetInstance();
  if (!keyboard_controller)
    return;

  // The layout manager for the root window propagates a resize to its
  // immediate children and grandchildren, but stops there. The keyboard
  // container is three levels deep, and therefore needs to be explicitly
  // updated when its parent is resized.
  aura::Window* keyboard_container =
      keyboard_controller->GetContainerWindow();
  if (keyboard_container)
    keyboard_container->SetBounds(parent_container_->bounds());
}

}  // namespace ash
