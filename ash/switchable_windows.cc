// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/switchable_windows.h"

#include "ash/shell_window_ids.h"

namespace ash {

const int kSwitchableWindowContainerIds[] = {
  internal::kShellWindowId_DefaultContainer,
  internal::kShellWindowId_AlwaysOnTopContainer,
  internal::kShellWindowId_PanelContainer
};

const size_t kSwitchableWindowContainerIdsLength =
    arraysize(kSwitchableWindowContainerIds);

}  // namespace ash
