// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/switchable_windows.h"

#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"

namespace ash {
namespace wm {

const int kSwitchableWindowContainerIds[] = {
    kShellWindowId_DefaultContainer, kShellWindowId_AlwaysOnTopContainer,
    kShellWindowId_DockedContainer, kShellWindowId_PanelContainer,
    kShellWindowId_AppListContainer};

const size_t kSwitchableWindowContainerIdsLength =
    arraysize(kSwitchableWindowContainerIds);

bool IsSwitchableContainer(const WmWindow* window) {
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
