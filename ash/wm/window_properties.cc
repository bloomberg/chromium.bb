// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/common/wm/window_state.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::wm::WindowState*);
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, ash::WidgetCreationType);

namespace ash {

DEFINE_WINDOW_PROPERTY_KEY(bool, kLockedToRootKey, false);

DEFINE_WINDOW_PROPERTY_KEY(bool, kPanelAttachedKey, true);

DEFINE_OWNED_WINDOW_PROPERTY_KEY(gfx::Rect, kRestoreBoundsOverrideKey, NULL);

DEFINE_WINDOW_PROPERTY_KEY(ui::WindowShowState,
                           kRestoreShowStateOverrideKey,
                           ui::SHOW_STATE_DEFAULT);

DEFINE_WINDOW_PROPERTY_KEY(ShelfID, kShelfIDKey, kInvalidShelfID);

DEFINE_WINDOW_PROPERTY_KEY(int32_t, kShelfItemTypeKey, TYPE_UNDEFINED);

DEFINE_WINDOW_PROPERTY_KEY(bool, kSnapChildrenToPixelBoundary, false);

DEFINE_WINDOW_PROPERTY_KEY(bool, kUsesScreenCoordinatesKey, false);

DEFINE_WINDOW_PROPERTY_KEY(WidgetCreationType,
                           kWidgetCreationTypeKey,
                           WidgetCreationType::INTERNAL);

DEFINE_OWNED_WINDOW_PROPERTY_KEY(wm::WindowState, kWindowStateKey, NULL);

}  // namespace ash
