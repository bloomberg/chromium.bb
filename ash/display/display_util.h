// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_UTIL_H_
#define ASH_DISPLAY_DISPLAY_UTIL_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_layout.h"

namespace gfx {
class Display;
class Point;
class Rect;
}

namespace ash {
class AshWindowTreeHost;
struct DisplayMode;
class DisplayInfo;

// Creates the display mode list for internal display
// based on |native_mode|.
ASH_EXPORT std::vector<DisplayMode> CreateInternalDisplayModeList(
    const DisplayMode& native_mode);

// Returns next valid UI scale.
float GetNextUIScale(const DisplayInfo& info, bool up);

// Tests if the |info| has display mode that matches |ui_scale|.
bool HasDisplayModeForUIScale(const DisplayInfo& info, float ui_scale);

// Computes the bounds that defines the bounds between two displays
// based on the layout |position|.
void ComputeBoundary(const gfx::Display& primary_display,
                     const gfx::Display& secondary_display,
                     DisplayLayout::Position position,
                     gfx::Rect* primary_edge_in_screen,
                     gfx::Rect* secondary_edge_in_screen);

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

// Returns the index in the displays whose bounds contains |point_in_screen|.
// Returns -1 if no such display exist.
ASH_EXPORT int FindDisplayIndexContainingPoint(
    const std::vector<gfx::Display>& displays,
    const gfx::Point& point_in_screen);

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_UTIL_H_
