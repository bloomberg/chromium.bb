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
#include "ui/display/manager/display_layout.h"

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace ash {
class AshWindowTreeHost;
struct DisplayMode;
class DisplayInfo;

// Creates the display mode list for internal display
// based on |native_mode|.
ASH_EXPORT std::vector<DisplayMode> CreateInternalDisplayModeList(
    const DisplayMode& native_mode);

// Creates the display mode list for unified display
// based on |native_mode| and |scales|.
ASH_EXPORT std::vector<DisplayMode> CreateUnifiedDisplayModeList(
    const DisplayMode& native_mode,
    const std::set<std::pair<float, float>>& dsf_scale_list);

// Gets the display mode for |resolution|. Returns false if no display
// mode matches the resolution, or the display is an internal display.
ASH_EXPORT bool GetDisplayModeForResolution(const DisplayInfo& info,
                                            const gfx::Size& resolution,
                                            DisplayMode* out);

// Gets the display mode for the next valid UI scale. Returns false
// if the display is not an internal display.
ASH_EXPORT bool GetDisplayModeForNextUIScale(const DisplayInfo& info,
                                             bool up,
                                             DisplayMode* out);

// Gets the display mode for the next valid resolution. Returns false
// if the display is an internal display.
ASH_EXPORT bool GetDisplayModeForNextResolution(const DisplayInfo& info,
                                                bool up,
                                                DisplayMode* out);

// Sets the UI scale for the |display_id|. Returns false if the
// display_id is not an internal display.
ASH_EXPORT bool SetDisplayUIScale(int64_t display_id, float scale);

// Tests if the |info| has display mode that matches |ui_scale|.
bool HasDisplayModeForUIScale(const DisplayInfo& info, float ui_scale);

// Computes the bounds that defines the bounds between two displays.
// Returns false if two displays do not intersect.
bool ComputeBoundary(const display::Display& primary_display,
                     const display::Display& secondary_display,
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
    const std::vector<display::Display>& displays,
    const gfx::Point& point_in_screen);

// Sorts id list using |CompareDisplayIds| below.
ASH_EXPORT void SortDisplayIdList(display::DisplayIdList* list);

// Default id generator.
class DefaultDisplayIdGenerator {
 public:
  int64_t operator()(int64_t id) { return id; }
};

// Generate sorted display::DisplayIdList from iterators.
template <class ForwardIterator, class Generator = DefaultDisplayIdGenerator>
display::DisplayIdList GenerateDisplayIdList(
    ForwardIterator first,
    ForwardIterator last,
    Generator generator = Generator()) {
  display::DisplayIdList list;
  while (first != last) {
    list.push_back(generator(*first));
    ++first;
  }
  SortDisplayIdList(&list);
  return list;
}

// Creates sorted display::DisplayIdList.
ASH_EXPORT display::DisplayIdList CreateDisplayIdList(
    const display::DisplayList& list);

ASH_EXPORT std::string DisplayIdListToString(
    const display::DisplayIdList& list);

// Returns true if one of following conditinos is met.
// 1) id1 is internal.
// 2) output index of id1 < output index of id2 and id2 isn't internal.
ASH_EXPORT bool CompareDisplayIds(int64_t id1, int64_t id2);

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_UTIL_H_
