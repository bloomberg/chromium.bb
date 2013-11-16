// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_util.h"

#include "ui/aura/window.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::LauncherID);

namespace ash {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(LauncherID, kLauncherID, 0);

void SetLauncherIDForWindow(LauncherID id, aura::Window* window) {
  if (!window)
    return;

  window->SetProperty(kLauncherID, id);
}

LauncherID GetLauncherIDForWindow(aura::Window* window) {
  DCHECK(window);
  return window->GetProperty(kLauncherID);
}

}  // namespace ash
