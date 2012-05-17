// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/wm/shadow_types.h"
#include "ui/aura/window_property.h"

// Property type for bool and ui::WindowShowState are
// defined in aura.
DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::ShadowType);
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowPersistsAcrossAllWorkspacesType)

namespace ash {
namespace internal {

DEFINE_WINDOW_PROPERTY_KEY(
    bool, kChildWindowVisibilityChangesAnimatedKey, false);
DEFINE_WINDOW_PROPERTY_KEY(
    ui::WindowShowState, kRestoreShowStateKey, ui::SHOW_STATE_DEFAULT);
DEFINE_WINDOW_PROPERTY_KEY(ShadowType, kShadowTypeKey, SHADOW_TYPE_NONE);
DEFINE_WINDOW_PROPERTY_KEY(ash::WindowPersistsAcrossAllWorkspacesType,
                           kWindowPersistsAcrossAllWorkspacesKey,
                           WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_DEFAULT);
DEFINE_WINDOW_PROPERTY_KEY(bool, kWindowTrackedByWorkspaceKey, true);

}  // namespace internal
}  // namespace ash
