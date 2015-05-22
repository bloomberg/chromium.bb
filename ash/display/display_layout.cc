// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_layout.h"

#include "ash/display/display_pref_util.h"
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
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";
const char kMirroredKey[] = "mirrored";
const char kPrimaryIdKey[] = "primary-id";

typedef std::map<DisplayLayout::Position, std::string> PositionToStringMap;

const PositionToStringMap* GetPositionToStringMap() {
  static const PositionToStringMap* map = CreateToStringMap(
      DisplayLayout::TOP, "top",
      DisplayLayout::BOTTOM, "bottom",
      DisplayLayout::RIGHT, "right",
      DisplayLayout::LEFT, "left");
  return map;
}

bool GetPositionFromString(const base::StringPiece& position,
                           DisplayLayout::Position* field) {
  if (ReverseFind(GetPositionToStringMap(), position, field))
    return true;
  LOG(ERROR) << "Invalid position value:" << position;
  return false;
}

std::string GetStringFromPosition(DisplayLayout::Position position) {
  const PositionToStringMap* map = GetPositionToStringMap();
  PositionToStringMap::const_iterator iter = map->find(position);
  return iter != map->end() ? iter->second : std::string("unknown");
}

bool GetDisplayIdFromString(const base::StringPiece& position, int64* field) {
  return base::StringToInt64(position, field);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DisplayLayout

// static
DisplayLayout DisplayLayout::FromInts(int position, int offsets) {
  return DisplayLayout(static_cast<Position>(position), offsets);
}

DisplayLayout::DisplayLayout()
    : position(RIGHT),
      offset(0),
      mirrored(false),
      primary_id(gfx::Display::kInvalidDisplayID) {
}

DisplayLayout::DisplayLayout(DisplayLayout::Position position, int offset)
    : position(position),
      offset(offset),
      mirrored(false),
      primary_id(gfx::Display::kInvalidDisplayID) {
  DCHECK_LE(TOP, position);
  DCHECK_GE(LEFT, position);

  // Set the default value to |position| in case position is invalid.  DCHECKs
  // above doesn't stop in Release builds.
  if (TOP > position || LEFT < position)
    this->position = RIGHT;

  DCHECK_GE(kMaxValidOffset, abs(offset));
}

DisplayLayout DisplayLayout::Invert() const {
  Position inverted_position = RIGHT;
  switch (position) {
    case TOP:
      inverted_position = BOTTOM;
      break;
    case BOTTOM:
      inverted_position = TOP;
      break;
    case RIGHT:
      inverted_position = LEFT;
      break;
    case LEFT:
      inverted_position = RIGHT;
      break;
  }
  DisplayLayout ret = DisplayLayout(inverted_position, -offset);
  ret.primary_id = primary_id;
  return ret;
}

// static
bool DisplayLayout::ConvertFromValue(const base::Value& value,
                                     DisplayLayout* layout) {
  base::JSONValueConverter<DisplayLayout> converter;
  return converter.Convert(value, layout);
}

// static
bool DisplayLayout::ConvertToValue(const DisplayLayout& layout,
                                   base::Value* value) {
  base::DictionaryValue* dict_value = NULL;
  if (!value->GetAsDictionary(&dict_value) || dict_value == NULL)
    return false;

  const std::string position_str = GetStringFromPosition(layout.position);
  dict_value->SetString(kPositionKey, position_str);
  dict_value->SetInteger(kOffsetKey, layout.offset);
  dict_value->SetBoolean(kMirroredKey, layout.mirrored);
  dict_value->SetString(kPrimaryIdKey, base::Int64ToString(layout.primary_id));
  return true;
}

std::string DisplayLayout::ToString() const {
  const std::string position_str = GetStringFromPosition(position);
  return base::StringPrintf(
      "%s, %d%s",
      position_str.c_str(), offset, mirrored ? ", mirrored" : "");
}

// static
void DisplayLayout::RegisterJSONConverter(
    base::JSONValueConverter<DisplayLayout>* converter) {
  converter->RegisterCustomField<Position>(
      kPositionKey, &DisplayLayout::position, &GetPositionFromString);
  converter->RegisterIntField(kOffsetKey, &DisplayLayout::offset);
  converter->RegisterBoolField(kMirroredKey, &DisplayLayout::mirrored);
  converter->RegisterCustomField<int64>(
      kPrimaryIdKey, &DisplayLayout::primary_id, &GetDisplayIdFromString);
}

}  // namespace ash
