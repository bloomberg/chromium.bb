// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/container_delegate_aura.h"

#include "ash/root_window_controller.h"
#include "ash/shell_window_ids.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

bool IsInContainer(views::Widget* widget, int container_id) {
  aura::Window* window = widget->GetNativeWindow();
  RootWindowController* root_controller =
      GetRootWindowController(window->GetRootWindow());
  if (!root_controller)
    return false;
  return root_controller->GetContainer(container_id)->Contains(window);
}

}  // namespace

ContainerDelegateAura::ContainerDelegateAura() {}

ContainerDelegateAura::~ContainerDelegateAura() {}

bool ContainerDelegateAura::IsInMenuContainer(views::Widget* widget) {
  return IsInContainer(widget, kShellWindowId_MenuContainer);
}

bool ContainerDelegateAura::IsInStatusContainer(views::Widget* widget) {
  return IsInContainer(widget, kShellWindowId_StatusContainer);
}

}  // namespace ash
