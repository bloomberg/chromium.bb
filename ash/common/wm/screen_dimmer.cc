// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/screen_dimmer.h"

#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/window_dimmer.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_user_data.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/memory/ptr_util.h"

namespace ash {
namespace {

// Opacity when it's dimming the entire screen.
const float kDimmingLayerOpacityForRoot = 0.4f;

// Opacity for lock screen.
const float kDimmingLayerOpacityForLockScreen = 0.5f;

}  // namespace

ScreenDimmer::ScreenDimmer(Container container)
    : container_(container),
      is_dimming_(false),
      at_bottom_(false),
      window_dimmers_(base::MakeUnique<WmWindowUserData<WindowDimmer>>()) {
  WmShell::Get()->AddShellObserver(this);
}

ScreenDimmer::~ScreenDimmer() {
  // Usage in chrome results in ScreenDimmer outliving the shell.
  if (WmShell::HasInstance())
    WmShell::Get()->RemoveShellObserver(this);
}

void ScreenDimmer::SetDimming(bool should_dim) {
  if (should_dim == is_dimming_)
    return;
  is_dimming_ = should_dim;

  Update(should_dim);
}

std::vector<WmWindow*> ScreenDimmer::GetAllContainers() {
  return container_ == Container::ROOT
             ? WmShell::Get()->GetAllRootWindows()
             : wm::GetContainersFromAllRootWindows(
                   ash::kShellWindowId_LockScreenContainersContainer);
}

void ScreenDimmer::OnRootWindowAdded(WmWindow* root_window) {
  Update(is_dimming_);
}

void ScreenDimmer::Update(bool should_dim) {
  for (WmWindow* container : GetAllContainers()) {
    WindowDimmer* window_dimmer = window_dimmers_->Get(container);
    if (should_dim) {
      if (!window_dimmer) {
        window_dimmers_->Set(container,
                             base::MakeUnique<WindowDimmer>(container));
        window_dimmer = window_dimmers_->Get(container);
        window_dimmer->SetDimOpacity(container_ == Container::ROOT
                                         ? kDimmingLayerOpacityForRoot
                                         : kDimmingLayerOpacityForLockScreen);
      }
      if (at_bottom_)
        container->StackChildAtBottom(window_dimmer->window());
      else
        container->StackChildAtTop(window_dimmer->window());
      window_dimmer->window()->Show();
    } else if (window_dimmer) {
      window_dimmers_->Set(container, nullptr);
    }
  }
}

}  // namespace ash
