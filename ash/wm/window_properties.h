// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PROPERTIES_H_
#define ASH_WM_WINDOW_PROPERTIES_H_

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
template <typename T>
struct WindowProperty;
}

namespace ash {
namespace wm {
class WindowState;
}  // namespace wm

// Shell-specific window property keys.

// Alphabetical sort.

// If this is set to true, the window stays in the same root window even if the
// bounds outside of its root window is set.
// This is exported as it's used in the tests.
ASH_EXPORT extern const aura::WindowProperty<bool>* const kLockedToRootKey;

// A property key which stores the bounds to restore a window to. These take
// preference over the current bounds/state. This is used by e.g. the always
// maximized mode window manager.
ASH_EXPORT extern const aura::WindowProperty<gfx::Rect*>* const
    kRestoreBoundsOverrideKey;

// A property key which stores the bounds to restore a window to. These take
// preference over the current bounds/state if |kRestoreBoundsOverrideKey| is
// set. This is used by e.g. the always maximized mode window manager.
ASH_EXPORT extern const aura::WindowProperty<ui::WindowShowState>* const
    kRestoreShowStateOverrideKey;

// A property key to store the id for a window's shelf item.
ASH_EXPORT extern const aura::WindowProperty<ShelfID>* const kShelfIDKey;

// A property key to store the type of a window's shelf item.
ASH_EXPORT extern const aura::WindowProperty<int>* const kShelfItemTypeKey;

// Containers with this property (true) are aligned with physical pixel
// boundary.
extern const aura::WindowProperty<bool>* const kSnapChildrenToPixelBoundary;

// Property to tell if the container uses the screen coordinates.
extern const aura::WindowProperty<bool>* const kUsesScreenCoordinatesKey;

// A property key to store WindowState in the window. The window state
// is owned by the window.
extern const aura::WindowProperty<wm::WindowState*>* const kWindowStateKey;

// Alphabetical sort.

}  // namespace ash

#endif  // ASH_WM_WINDOW_PROPERTIES_H_
