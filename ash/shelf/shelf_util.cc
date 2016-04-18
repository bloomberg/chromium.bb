// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_util.h"

#include "ash/shelf/shelf_constants.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::ShelfItemDetails*);

namespace ash {

DEFINE_WINDOW_PROPERTY_KEY(ShelfID, kShelfID, kInvalidShelfID);

// ShelfItemDetails for kShelfItemDetaildKey is owned by the window
// and will be freed automatically.
DEFINE_OWNED_WINDOW_PROPERTY_KEY(ShelfItemDetails,
                                 kShelfItemDetailsKey,
                                 NULL);

void SetShelfIDForWindow(ShelfID id, aura::Window* window) {
  if (!window)
    return;

  window->SetProperty(kShelfID, id);
}

ShelfID GetShelfIDForWindow(const aura::Window* window) {
  DCHECK(window);
  return window->GetProperty(kShelfID);
}

void SetShelfItemDetailsForWindow(aura::Window* window,
                                  const ShelfItemDetails& details) {
  // |item_details| is owned by |window|.
  ShelfItemDetails* item_details = new ShelfItemDetails(details);
  window->SetProperty(kShelfItemDetailsKey, item_details);
}

void SetShelfItemDetailsForDialogWindow(aura::Window* window,
                                        int image_resource_id,
                                        const base::string16& title) {
  // |item_details| is owned by |window|.
  ShelfItemDetails* item_details = new ShelfItemDetails;
  item_details->type = TYPE_DIALOG;
  item_details->image_resource_id = image_resource_id;
  item_details->title = title;
  window->SetProperty(kShelfItemDetailsKey, item_details);
}

void ClearShelfItemDetailsForWindow(aura::Window* window) {
  window->ClearProperty(kShelfItemDetailsKey);
}

const ShelfItemDetails* GetShelfItemDetailsForWindow(
    aura::Window* window) {
  return window->GetProperty(kShelfItemDetailsKey);
}

bool IsHorizontalAlignment(ShelfAlignment alignment) {
  return alignment == SHELF_ALIGNMENT_BOTTOM ||
         alignment == SHELF_ALIGNMENT_BOTTOM_LOCKED;
}

}  // namespace ash
