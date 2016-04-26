// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_UTIL_H_
#define ASH_SHELF_SHELF_UTIL_H_

#include "ash/ash_export.h"
#include "ash/shelf/shelf_item_types.h"
#include "ash/shelf/shelf_types.h"
#include "base/strings/string16.h"
#include "ui/aura/window.h"

namespace aura {
class Window;
}

namespace ash {

// A property key to store the id of the ShelfItem associated with the window.
extern const aura::WindowProperty<ShelfID>* const kShelfID;

// A property key to store the resource id and title of the item shown on the
// shelf for this window.
extern const aura::WindowProperty<ShelfItemDetails*>* const
    kShelfItemDetailsKey;

// Associates ShelfItem of |id| with specified |window|.
ASH_EXPORT void SetShelfIDForWindow(ShelfID id, aura::Window* window);

// Returns the id of the ShelfItem associated with the specified |window|,
// or 0 if there isn't one.
// Note: Window of a tabbed browser will return the |ShelfID| of the
// currently active tab.
ASH_EXPORT ShelfID GetShelfIDForWindow(const aura::Window* window);

// Creates a new ShelfItemDetails instance from |details| and sets it for
// |window|.
ASH_EXPORT void SetShelfItemDetailsForWindow(aura::Window* window,
                                             const ShelfItemDetails& details);

// Creates a new ShelfItemDetails instance with type DIALOG, image id
// |image_resource_id|, and title |title|, and sets it for |window|.
ASH_EXPORT void SetShelfItemDetailsForDialogWindow(aura::Window* window,
                                                   int image_resource_id,
                                                   const base::string16& title);

// Clears ShelfItemDetails for |window|.
// If |window| has a ShelfItem by SetShelfItemDetailsForWindow(), it will
// be removed.
ASH_EXPORT void ClearShelfItemDetailsForWindow(aura::Window* window);

// Returns ShelfItemDetails for |window| or NULL if it doesn't have.
// Returned ShelfItemDetails object is owned by the |window|.
ASH_EXPORT const ShelfItemDetails* GetShelfItemDetailsForWindow(
    aura::Window* window);

// Returns true if the shelf |alignment| is horizontal.
ASH_EXPORT bool IsHorizontalAlignment(wm::ShelfAlignment alignment);

}  // namespace ash

#endif  // ASH_SHELF_SHELF_UTIL_H_
