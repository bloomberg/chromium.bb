// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/wm/window_state.h"
#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::wm::WindowState*);
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, gfx::Rect*)
DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, ui::WindowShowState)

namespace ash {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(gfx::Rect,
                                 kRestoreBoundsOverrideKey,
                                 NULL);

DEFINE_WINDOW_PROPERTY_KEY(ui::WindowShowState,
                           kRestoreShowStateOverrideKey,
                           ui::SHOW_STATE_DEFAULT);

DEFINE_WINDOW_PROPERTY_KEY(bool, kSnapChildrenToPixelBoundary, false);

DEFINE_WINDOW_PROPERTY_KEY(bool, kStayInSameRootWindowKey, false);

DEFINE_WINDOW_PROPERTY_KEY(bool, kUsesScreenCoordinatesKey, false);

DEFINE_OWNED_WINDOW_PROPERTY_KEY(wm::WindowState,
                                 kWindowStateKey, NULL);

}  // namespace ash
