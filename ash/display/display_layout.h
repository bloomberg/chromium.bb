// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_LAYOUT_H_
#define ASH_DISPLAY_DISPLAY_LAYOUT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"

namespace base {
class Value;
template <typename T> class JSONValueConverter;
}

namespace ash {

// An identifier used to manage display layout in DisplayManager /
// DisplayLayoutStore.
using DisplayIdList = std::vector<int64_t>;

// DisplayPlacement specifies where the display (D) is placed relative
// to parent (P) display.  In the following example, the display (D)
// given by |display_id| is placed at the left side of the parent
// display (P) given by |parent_display_id|, with a negative offset.
//
//        +      +--------+
// offset |      |        |
//        +      |   D    +--------+
//               |        |        |
//               +--------+   P    |
//                        |        |
//                        +--------+
//
struct ASH_EXPORT DisplayPlacement {
  // The id of the display this placement will be applied to.
  int64_t display_id;

  // The parent display id to which the above display is placed.
  int64_t parent_display_id;

  // To which side the parent display the display is positioned.
  enum Position { TOP, RIGHT, BOTTOM, LEFT };
  Position position;

  // The offset of the position of the secondary display. The offset is
  // based on the top/left edge of the primary display.
  int offset;

  DisplayPlacement(Position position, int offset);

  DisplayPlacement& Swap();

  std::string ToString() const;
};

struct ASH_EXPORT DisplayLayout {
  DisplayLayout();
  ~DisplayLayout();

  // Converter functions to/from base::Value.
  static bool ConvertFromValue(const base::Value& value, DisplayLayout* layout);
  static bool ConvertToValue(const DisplayLayout& layout, base::Value* value);

  static void RegisterJSONConverter(
      base::JSONValueConverter<DisplayLayout>* converter);

  DisplayPlacement placement;

  // True if displays are mirrored.
  bool mirrored;

  // True if multi displays should default to unified mode.
  bool default_unified;

  // The id of the display used as a primary display.
  int64_t primary_id;

  // Returns string representation of the layout for debugging/testing.
  // This includes "unified" only if the unified desktop feature is enabled.
  std::string ToString() const;
};

}  // namespace ash

#endif
