// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_util.h"

#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::LauncherID);
DECLARE_WINDOW_PROPERTY_TYPE(ash::LauncherItemDetails*);

namespace ash {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(LauncherID, kLauncherID, kInvalidLauncherID);

// ash::LauncherItemDetails for kLauncherItemDetaildKey is owned by the window
// and will be freed automatically.
DEFINE_OWNED_WINDOW_PROPERTY_KEY(LauncherItemDetails,
                                 kLauncherItemDetailsKey,
                                 NULL);

void SetLauncherIDForWindow(LauncherID id, aura::Window* window) {
  if (!window)
    return;

  window->SetProperty(kLauncherID, id);
}

LauncherID GetLauncherIDForWindow(aura::Window* window) {
  DCHECK(window);
  return window->GetProperty(kLauncherID);
}

void SetLauncherItemDetailsForWindow(aura::Window* window,
                                     const LauncherItemDetails& details) {
  // |item_details| is owned by |window|.
  LauncherItemDetails* item_details = new LauncherItemDetails(details);
  window->SetProperty(kLauncherItemDetailsKey, item_details);
}

void ClearLauncherItemDetailsForWindow(aura::Window* window) {
  window->ClearProperty(kLauncherItemDetailsKey);
}

const LauncherItemDetails* GetLauncherItemDetailsForWindow(
    aura::Window* window) {
  return window->GetProperty(kLauncherItemDetailsKey);
}

}  // namespace ash
