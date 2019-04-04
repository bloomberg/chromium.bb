// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_util.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ui/aura/window.h"

namespace ash {

namespace desks_util {

bool IsDeskContainerId(int id) {
  // TODO(afakhry): Add the rest of the desks containers.
  return id == kShellWindowId_DefaultContainer;
}

int GetActiveDeskContainerId() {
  // TODO(afakhry): Do proper checking when the other desks containers are
  // added.
  return kShellWindowId_DefaultContainer;
}

ASH_EXPORT bool IsActiveDeskContainer(aura::Window* container) {
  DCHECK(container);
  return container->id() == GetActiveDeskContainerId();
}

aura::Window* GetActiveDeskContainerForRoot(aura::Window* root) {
  DCHECK(root);
  return root->GetChildById(GetActiveDeskContainerId());
}

}  // namespace desks_util

}  // namespace ash
