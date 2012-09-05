// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/root_window_controller.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/shadow_types.h"
#include "ui/aura/window_property.h"
#include "ui/ui_controls/ui_controls_aura.h"

// Property type for bool and ui::WindowShowState are
// defined in aura.
DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::AlwaysOnTopController*);
DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::ShadowType);
DECLARE_WINDOW_PROPERTY_TYPE(ash::WindowPersistsAcrossAllWorkspacesType)
DECLARE_WINDOW_PROPERTY_TYPE(ui_controls::UIControlsAura*)
DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::RootWindowController*);

namespace ash {
namespace internal {
DEFINE_OWNED_WINDOW_PROPERTY_KEY(ash::internal::AlwaysOnTopController,
                                 kAlwaysOnTopControllerKey,
                                 NULL);
DEFINE_WINDOW_PROPERTY_KEY(
    bool, kChildWindowVisibilityChangesAnimatedKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kIgnoredByShelfKey, false);
DEFINE_WINDOW_PROPERTY_KEY(
    ui::WindowShowState, kRestoreShowStateKey, ui::SHOW_STATE_DEFAULT);
DEFINE_WINDOW_PROPERTY_KEY(RootWindowController*,
                           kRootWindowControllerKey, NULL);
DEFINE_WINDOW_PROPERTY_KEY(ShadowType, kShadowTypeKey, SHADOW_TYPE_NONE);
DEFINE_OWNED_WINDOW_PROPERTY_KEY(ui_controls::UIControlsAura,
                                 kUIControlsKey,
                                 NULL);
DEFINE_WINDOW_PROPERTY_KEY(bool, kUsesScreenCoordinatesKey, false);
DEFINE_WINDOW_PROPERTY_KEY(ash::WindowPersistsAcrossAllWorkspacesType,
                           kWindowPersistsAcrossAllWorkspacesKey,
                           WINDOW_PERSISTS_ACROSS_ALL_WORKSPACES_VALUE_DEFAULT);
DEFINE_WINDOW_PROPERTY_KEY(bool, kWindowTrackedByWorkspaceKey, true);

}  // namespace internal
}  // namespace ash
