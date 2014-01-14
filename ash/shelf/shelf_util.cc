// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_util.h"

#include "ash/shelf/shelf_constants.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::LauncherID);
DECLARE_WINDOW_PROPERTY_TYPE(ash::ShelfItemDetails*);

namespace ash {

DEFINE_LOCAL_WINDOW_PROPERTY_KEY(LauncherID, kLauncherID, kInvalidShelfID);

// ShelfItemDetails for kLauncherItemDetaildKey is owned by the window
// and will be freed automatically.
DEFINE_OWNED_WINDOW_PROPERTY_KEY(ShelfItemDetails,
                                 kShelfItemDetailsKey,
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

void SetShelfItemDetailsForWindow(aura::Window* window,
                                  const ShelfItemDetails& details) {
  // |item_details| is owned by |window|.
  ShelfItemDetails* item_details = new ShelfItemDetails(details);
  window->SetProperty(kShelfItemDetailsKey, item_details);
}

void ClearShelfItemDetailsForWindow(aura::Window* window) {
  window->ClearProperty(kShelfItemDetailsKey);
}

const ShelfItemDetails* GetShelfItemDetailsForWindow(
    aura::Window* window) {
  return window->GetProperty(kShelfItemDetailsKey);
}

}  // namespace ash
