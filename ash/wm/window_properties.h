// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_PROPERTIES_H_
#define ASH_WM_WINDOW_PROPERTIES_H_
#pragma once

#include "ash/wm/property_util.h"
#include "ash/wm/shadow_types.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"

namespace ash {
namespace internal {

// Shell-specific window property keys.

// Alphabetical sort.

// Property set on all windows whose child windows' visibility changes are
// animated.
extern const aura::WindowProperty<bool>* const
    kChildWindowVisibilityChangesAnimatedKey;

// Used to remember the show state before the window was minimized.
extern const aura::WindowProperty<ui::WindowShowState>* const
    kRestoreShowStateKey;

// A property key describing the drop shadow that should be displayed under the
// window.  If unset, no shadow is displayed.
extern const aura::WindowProperty<ShadowType>* const kShadowTypeKey;

extern const aura::WindowProperty<WindowPersistsAcrossAllWorkspacesType>* const
    kWindowPersistsAcrossAllWorkspacesKey;

// True if the window is controlled by the workspace manager.
extern const aura::WindowProperty<bool>* const
    kWindowTrackedByWorkspaceKey;

// Alphabetical sort.

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WINDOW_PROPERTIES_H_
