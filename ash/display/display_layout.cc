// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_layout.h"

#include <algorithm>
#include <sstream>

#include "ash/ash_switches.h"
#include "ash/display/display_pref_util.h"
#include "ash/shell.h"
#include "base/json/json_value_converter.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "ui/gfx/display.h"

namespace ash {
namespace  {

// The maximum value for 'offset' in DisplayLayout in case of outliers.  Need
// to change this value in case to support even larger displays.
const int kMaxValidOffset = 10000;

// Persistent key names
const char kMirroredKey[] = "mirrored";
const char kDefaultUnifiedKey[] = "default_unified";
const char kPrimaryIdKey[] = "primary-id";
const char kDisplayPlacementKey[] = "display_placement";

// DisplayPlacement
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";
const char kDisplayPlacementDisplayIdKey[] = "display_id";
const char kDisplayPlacementParentDisplayIdKey[] = "parent_display_id";

using PositionToStringMap = std::map<DisplayPlacement::Position, std::string>;
using DisplayPlacementMap = std::unordered_map<int64_t, DisplayPlacement>;

const PositionToStringMap* GetPositionToStringMap() {
  static const PositionToStringMap* map = CreateToStringMap(
      DisplayPlacement::TOP, "top", DisplayPlacement::BOTTOM, "bottom",
      DisplayPlacement::RIGHT, "right", DisplayPlacement::LEFT, "left");
  return map;
}

std::string ToPositionString(DisplayPlacement::Position position) {
  const PositionToStringMap* map = GetPositionToStringMap();
  PositionToStringMap::const_iterator iter = map->find(position);
  return iter != map->end() ? iter->second : std::string("unknown");
}

bool GetPositionFromString(const base::StringPiece& position,
                           DisplayPlacement::Position* field) {
  if (ReverseFind(GetPositionToStringMap(), position, field))
    return true;
  LOG(ERROR) << "Invalid position value:" << position;
  return false;
}

bool GetDisplayIdFromString(const base::StringPiece& position, int64_t* field) {
  return base::StringToInt64(position, field);
}

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
  s << ToPositionString(position) << ", ";
  s << offset;
  return s.str();
}

// static
void DisplayPlacement::RegisterJSONConverter(
    base::JSONValueConverter<DisplayPlacement>* converter) {
  converter->RegisterIntField(kOffsetKey, &DisplayPlacement::offset);
  converter->RegisterCustomField<DisplayPlacement::Position>(
      kPositionKey, &DisplayPlacement::position, &GetPositionFromString);
  converter->RegisterCustomField<int64_t>(kDisplayPlacementDisplayIdKey,
                                          &DisplayPlacement::display_id,
                                          &GetDisplayIdFromString);
  converter->RegisterCustomField<int64_t>(kDisplayPlacementParentDisplayIdKey,
                                          &DisplayPlacement::parent_display_id,
                                          &GetDisplayIdFromString);
}

////////////////////////////////////////////////////////////////////////////////
// DisplayLayout

DisplayLayout::DisplayLayout()
    : mirrored(false),
      default_unified(true),
      primary_id(gfx::Display::kInvalidDisplayID) {}

DisplayLayout::~DisplayLayout() {}

// static
bool DisplayLayout::ConvertFromValue(const base::Value& value,
                                     DisplayLayout* layout) {
  layout->placement_list.clear();
  base::JSONValueConverter<DisplayLayout> converter;
  if (!converter.Convert(value, layout))
    return false;
  if (layout->placement_list.size() != 0u)
    return true;
  // For compatibility with old format.
  const base::DictionaryValue* dict_value = nullptr;
  if (!value.GetAsDictionary(&dict_value) || dict_value == nullptr)
    return false;
  int offset;
  if (dict_value->GetInteger(kOffsetKey, &offset)) {
    DisplayPlacement::Position position;
    std::string position_str;
    if (!dict_value->GetString(kPositionKey, &position_str))
      return false;
    GetPositionFromString(position_str, &position);
    layout->placement_list.push_back(new DisplayPlacement(position, offset));
  }
  return true;
}

// static
bool DisplayLayout::ConvertToValue(const DisplayLayout& layout,
                                   base::Value* value) {
  base::DictionaryValue* dict_value = nullptr;
  if (!value->GetAsDictionary(&dict_value) || dict_value == nullptr)
    return false;

  dict_value->SetBoolean(kMirroredKey, layout.mirrored);
  dict_value->SetBoolean(kDefaultUnifiedKey, layout.default_unified);
  dict_value->SetString(kPrimaryIdKey, base::Int64ToString(layout.primary_id));

  scoped_ptr<base::ListValue> placement_list(new base::ListValue);
  for (const auto* placement : layout.placement_list) {
    scoped_ptr<base::DictionaryValue> placement_value(
        new base::DictionaryValue);
    placement_value->SetString(kPositionKey,
                               ToPositionString(placement->position));
    placement_value->SetInteger(kOffsetKey, placement->offset);
    placement_value->SetString(kDisplayPlacementDisplayIdKey,
                               base::Int64ToString(placement->display_id));
    placement_value->SetString(
        kDisplayPlacementParentDisplayIdKey,
        base::Int64ToString(placement->parent_display_id));
    placement_list->Append(std::move(placement_value));
  }
  dict_value->Set(kDisplayPlacementKey, std::move(placement_list));
  return true;
}

// static
void DisplayLayout::RegisterJSONConverter(
    base::JSONValueConverter<DisplayLayout>* converter) {
  converter->RegisterBoolField(kMirroredKey, &DisplayLayout::mirrored);
  converter->RegisterBoolField(kDefaultUnifiedKey,
                               &DisplayLayout::default_unified);
  converter->RegisterCustomField<int64_t>(
      kPrimaryIdKey, &DisplayLayout::primary_id, &GetDisplayIdFromString);
  converter->RegisterRepeatedMessage<DisplayPlacement>(
      kDisplayPlacementKey, &DisplayLayout::placement_list);
}

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
