// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_UTIL_H_
#define ASH_DISPLAY_DISPLAY_UTIL_H_

#include <stdint.h>

#include <set>
#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "base/memory/ref_counted.h"
#include "ui/display/manager/display_layout.h"
#include "ui/display/manager/managed_display_info.h"

namespace display {
class ManagedDisplayInfo;
}

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace ui {}

namespace ash {
class AshWindowTreeHost;

// TODO(rjkroege): Move this into display_manager.h
// Sets the UI scale for the |display_id|. Returns false if the
// display_id is not an internal display.
ASH_EXPORT bool SetDisplayUIScale(int64_t display_id, float scale);

// Creates edge bounds from |bounds_in_screen| that fits the edge
// of the native window for |ash_host|.
ASH_EXPORT gfx::Rect GetNativeEdgeBounds(AshWindowTreeHost* ash_host,
                                         const gfx::Rect& bounds_in_screen);

// Moves the cursor to the point inside the |ash_host| that is closest to
// the point_in_screen, which may be outside of the root window.
// |update_last_loation_now| is used for the test to update the mouse
// location synchronously.
void MoveCursorTo(AshWindowTreeHost* ash_host,
                  const gfx::Point& point_in_screen,
                  bool update_last_location_now);

#if defined(OS_CHROMEOS)
// Shows the notification message for display related issues.
void ShowDisplayErrorNotification(int message_id);
#endif

ASH_EXPORT base::string16 GetDisplayErrorNotificationMessageForTest();

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_UTIL_H_
