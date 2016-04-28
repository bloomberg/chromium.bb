// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/common/switchable_windows.h"

#include "ash/wm/common/wm_shell_window_ids.h"
#include "ash/wm/common/wm_window.h"

namespace ash {
namespace wm {

const int kSwitchableWindowContainerIds[] = {
    kShellWindowId_DefaultContainer, kShellWindowId_AlwaysOnTopContainer,
    kShellWindowId_DockedContainer, kShellWindowId_PanelContainer,
    kShellWindowId_AppListContainer};

const size_t kSwitchableWindowContainerIdsLength =
    arraysize(kSwitchableWindowContainerIds);

bool IsSwitchableContainer(const wm::WmWindow* window) {
  if (!window)
    return false;
  const int shell_window_id = window->GetShellWindowId();
  for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
    if (shell_window_id == kSwitchableWindowContainerIds[i])
      return true;
  }
  return false;
}

}  // namespace wm
}  // namespace ash
