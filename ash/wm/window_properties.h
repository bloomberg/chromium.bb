// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PROPERTIES_H_
#define ASH_WM_WINDOW_PROPERTIES_H_

#include "ash/wm/property_util.h"
#include "ash/wm/shadow_types.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ui_controls {
class UIControlsAura;
}

namespace ash {
namespace internal {
class AlwaysOnTopController;
class RootWindowController;

// Shell-specific window property keys.

// Alphabetical sort.

// A Key to store AlwaysOnTopController per RootWindow. The value is
// owned by the RootWindow.
extern const aura::WindowProperty<internal::AlwaysOnTopController*>* const
    kAlwaysOnTopControllerKey;

// Property set on all windows whose child windows' visibility changes are
// animated.
extern const aura::WindowProperty<bool>* const
    kChildWindowVisibilityChangesAnimatedKey;

// True if the window is ignored by the shelf layout manager for purposes of
// darkening the shelf.
extern const aura::WindowProperty<bool>* const
    kIgnoredByShelfKey;

// Used to remember the show state before the window was minimized.
extern const aura::WindowProperty<ui::WindowShowState>* const
    kRestoreShowStateKey;

extern const aura::WindowProperty<RootWindowController*>* const
    kRootWindowControllerKey;

// A property key describing the drop shadow that should be displayed under the
// window.  If unset, no shadow is displayed.
extern const aura::WindowProperty<ShadowType>* const kShadowTypeKey;

// Used to store a ui_controls for each root window.
extern const aura::WindowProperty<ui_controls::UIControlsAura*>* const
    kUIControlsKey;

// Property to tell if the container uses the screen coordinates.
extern const aura::WindowProperty<bool>* const kUsesScreenCoordinatesKey;

extern const aura::WindowProperty<WindowPersistsAcrossAllWorkspacesType>* const
    kWindowPersistsAcrossAllWorkspacesKey;

// True if the window is controlled by the workspace manager.
extern const aura::WindowProperty<bool>* const
    kWindowTrackedByWorkspaceKey;

// Alphabetical sort.

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WINDOW_PROPERTIES_H_
