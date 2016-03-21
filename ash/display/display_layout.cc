// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_layout.h"

#include <algorithm>
#include <sstream>

#include "ash/ash_switches.h"
#include "ash/display/display_pref_util.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "ui/gfx/display.h"

namespace ash {
namespace  {

// DisplayPlacement Positions
const char kTop[] = "top";
const char kRight[] = "right";
const char kBottom[] = "bottom";
const char kLeft[] = "left";
const char kUnknown[] = "unknown";

// The maximum value for 'offset' in DisplayLayout in case of outliers.  Need
// to change this value in case to support even larger displays.
const int kMaxValidOffset = 10000;

bool IsIdInList(int64_t id, const DisplayIdList& list) {
  const auto iter =
      std::find_if(list.begin(), list.end(),
                   [id](int64_t display_id) { return display_id == id; });
  return iter != list.end();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisplayPlacement

DisplayPlacement::DisplayPlacement()
    : display_id(gfx::Display::kInvalidDisplayID),
      parent_display_id(gfx::Display::kInvalidDisplayID),
      position(DisplayPlacement::RIGHT),
      offset(0) {}

DisplayPlacement::DisplayPlacement(const DisplayPlacement& placement)
    : display_id(placement.display_id),
      parent_display_id(placement.parent_display_id),
      position(placement.position),
      offset(placement.offset) {}

DisplayPlacement::DisplayPlacement(Position pos, int offset)
    : display_id(gfx::Display::kInvalidDisplayID),
      parent_display_id(gfx::Display::kInvalidDisplayID),
      position(pos),
      offset(offset) {
  DCHECK_LE(TOP, position);
  DCHECK_GE(LEFT, position);
  // Set the default value to |position| in case position is invalid.  DCHECKs
  // above doesn't stop in Release builds.
  if (TOP > position || LEFT < position)
    this->position = RIGHT;

  DCHECK_GE(kMaxValidOffset, abs(offset));
}

DisplayPlacement& DisplayPlacement::Swap() {
  switch (position) {
    case TOP:
      position = BOTTOM;
      break;
    case BOTTOM:
      position = TOP;
      break;
    case RIGHT:
      position = LEFT;
      break;
    case LEFT:
      position = RIGHT;
      break;
  }
  offset = -offset;
  std::swap(display_id, parent_display_id);
  return *this;
}

std::string DisplayPlacement::ToString() const {
  std::stringstream s;
  if (display_id != gfx::Display::kInvalidDisplayID)
    s << "id=" << display_id << ", ";
  if (parent_display_id != gfx::Display::kInvalidDisplayID)
    s << "parent=" << parent_display_id << ", ";
  s << PositionToString(position) << ", ";
  s << offset;
  return s.str();
}

// static
std::string DisplayPlacement::PositionToString(Position position) {
  switch (position) {
    case TOP:
      return kTop;
    case RIGHT:
      return kRight;
    case BOTTOM:
      return kBottom;
    case LEFT:
      return kLeft;
  }
  return kUnknown;
}

// static
bool DisplayPlacement::StringToPosition(const base::StringPiece& string,
                                        Position* position) {
  if (string == kTop) {
    *position = TOP;
    return true;
  }

  if (string == kRight) {
    *position = RIGHT;
    return true;
  }

  if (string == kBottom) {
    *position = BOTTOM;
    return true;
  }

  if (string == kLeft) {
    *position = LEFT;
    return true;
  }

  LOG(ERROR) << "Invalid position value:" << string;

  return false;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayLayout

DisplayLayout::DisplayLayout()
    : mirrored(false),
      default_unified(true),
      primary_id(gfx::Display::kInvalidDisplayID) {}

DisplayLayout::~DisplayLayout() {}

// static
bool DisplayLayout::Validate(const DisplayIdList& list,
                             const DisplayLayout& layout) {
  // The primary display should be in the list.
  DCHECK(IsIdInList(layout.primary_id, list));

  // Unified mode, or mirror mode switched from unified mode,
  // may not have the placement yet.
  if (layout.placement_list.size() == 0u)
    return true;

  bool has_primary_as_parent = false;
  int64_t id = 0;

  for (const auto* placement : layout.placement_list) {
    // Placements are sorted by display_id.
    if (id >= placement->display_id) {
      LOG(ERROR) << "PlacementList must be sorted by display_id";
      return false;
    }
    if (placement->display_id == gfx::Display::kInvalidDisplayID) {
      LOG(ERROR) << "display_id is not initialized";
      return false;
    }
    if (placement->parent_display_id == gfx::Display::kInvalidDisplayID) {
      LOG(ERROR) << "display_parent_id is not initialized";
      return false;
    }
    if (placement->display_id == placement->parent_display_id) {
      LOG(ERROR) << "display_id must not be same as parent_display_id";
      return false;
    }
    if (!IsIdInList(placement->display_id, list)) {
      LOG(ERROR) << "display_id is not in the id list:"
                 << placement->ToString();
      return false;
    }

    if (!IsIdInList(placement->parent_display_id, list)) {
      LOG(ERROR) << "parent_display_id is not in the id list:"
                 << placement->ToString();
      return false;
    }
    has_primary_as_parent |= layout.primary_id == placement->parent_display_id;
  }
  if (!has_primary_as_parent)
    LOG(ERROR) << "At least, one placement must have the primary as a parent.";
  return has_primary_as_parent;
}

scoped_ptr<DisplayLayout> DisplayLayout::Copy() const {
  scoped_ptr<DisplayLayout> copy(new DisplayLayout);
  for (auto placement : placement_list)
    copy->placement_list.push_back(new DisplayPlacement(*placement));
  copy->mirrored = mirrored;
  copy->default_unified = default_unified;
  copy->primary_id = primary_id;
  return copy;
}

bool DisplayLayout::HasSamePlacementList(const DisplayLayout& layout) const {
  if (placement_list.size() != layout.placement_list.size())
    return false;
  for (size_t index = 0; index < placement_list.size(); index++) {
    const DisplayPlacement& placement1 = *placement_list[index];
    const DisplayPlacement& placement2 = *layout.placement_list[index];
    if (placement1.position != placement2.position ||
        placement1.offset != placement2.offset ||
        placement1.display_id != placement2.display_id ||
        placement1.parent_display_id != placement2.parent_display_id) {
      return false;
    }
  }
  return true;
}

std::string DisplayLayout::ToString() const {
  std::stringstream s;
  s << "primary=" << primary_id;
  if (mirrored)
    s << ", mirrored";
  if (default_unified)
    s << ", default_unified";
  bool added = false;
  for (const auto* placement : placement_list) {
    s << (added ? "),(" : " [(");
    s << placement->ToString();
    added = true;
  }
  if (added)
    s << ")]";
  return s.str();
}

const DisplayPlacement* DisplayLayout::FindPlacementById(
    int64_t display_id) const {
  const auto iter =
      std::find_if(placement_list.begin(), placement_list.end(),
                   [display_id](const DisplayPlacement* placement) {
                     return placement->display_id == display_id;
                   });
  return (iter == placement_list.end()) ? nullptr : *iter;
}

}  // namespace ash
