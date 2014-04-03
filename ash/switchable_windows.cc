// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/switchable_windows.h"

#include "ash/shell_window_ids.h"
#include "ui/aura/window.h"

namespace ash {

const int kSwitchableWindowContainerIds[] = {
    kShellWindowId_DefaultContainer, kShellWindowId_AlwaysOnTopContainer,
    kShellWindowId_PanelContainer};

const size_t kSwitchableWindowContainerIdsLength =
    arraysize(kSwitchableWindowContainerIds);

bool IsSwitchableContainer(const aura::Window* window) {
  if (!window)
    return false;
  for (size_t i = 0; i < kSwitchableWindowContainerIdsLength; ++i) {
    if (window->id() == kSwitchableWindowContainerIds[i])
      return true;
  }
  return false;
}

}  // namespace ash
