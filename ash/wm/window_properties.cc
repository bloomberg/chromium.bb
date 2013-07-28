// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ash/root_window_controller.h"
#include "ash/wm/frame_painter.h"
#include "ui/aura/window_property.h"
#include "ui/gfx/rect.h"

DECLARE_EXPORTED_WINDOW_PROPERTY_TYPE(ASH_EXPORT, ash::FramePainter*);
DECLARE_WINDOW_PROPERTY_TYPE(ash::internal::RootWindowController*);

namespace ash {
namespace internal {
DEFINE_WINDOW_PROPERTY_KEY(bool, kContinueDragAfterReparent, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kFullscreenUsesMinimalChromeKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kIgnoreSoloWindowFramePainterPolicy, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kIgnoredByShelfKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kPanelAttachedKey, true);
DEFINE_WINDOW_PROPERTY_KEY(RootWindowController*,
                           kRootWindowControllerKey, NULL);
DEFINE_WINDOW_PROPERTY_KEY(bool, kSoloWindowHeaderKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kStayInSameRootWindowKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kUsesScreenCoordinatesKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kUserChangedWindowPositionOrSizeKey, false);
DEFINE_OWNED_WINDOW_PROPERTY_KEY(gfx::Rect,
                                 kPreAutoManagedWindowBoundsKey,
                                 NULL);
DEFINE_WINDOW_PROPERTY_KEY(bool, kWindowPositionManagedKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kWindowRestoresToRestoreBounds, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kWindowTrackedByWorkspaceKey, true);

}  // namespace internal
}  // namespace ash
