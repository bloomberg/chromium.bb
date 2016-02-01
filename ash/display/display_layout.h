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

typedef std::vector<int64_t> DisplayIdList;

struct ASH_EXPORT DisplayLayout {
  // Layout options where the secondary display should be positioned.
  enum Position {
    TOP,
    RIGHT,
    BOTTOM,
    LEFT
  };

  // Factory method to create DisplayLayout from ints. The |mirrored| is
  // set to false and |primary_id| is set to gfx::Display::kInvalidDisplayId.
  // Used for persistence and webui.
  static DisplayLayout FromInts(int position, int offsets);

  DisplayLayout();
  DisplayLayout(Position position, int offset);

  // Returns an inverted display layout.
  DisplayLayout Invert() const WARN_UNUSED_RESULT;

  // Converter functions to/from base::Value.
  static bool ConvertFromValue(const base::Value& value, DisplayLayout* layout);
  static bool ConvertToValue(const DisplayLayout& layout, base::Value* value);

  // This method is used by base::JSONValueConverter, you don't need to call
  // this directly. Instead consider using converter functions above.
  static void RegisterJSONConverter(
      base::JSONValueConverter<DisplayLayout>* converter);

  Position position;

  // The offset of the position of the secondary display.  The offset is
  // based on the top/left edge of the primary display.
  int offset;

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
