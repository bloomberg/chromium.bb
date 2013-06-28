// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_LAYOUT_H_
#define ASH_DISPLAY_DISPLAY_LAYOUT_H_

#include <map>
#include <string>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace base {
class Value;
template <typename T> class JSONValueConverter;
}

namespace ash {

typedef std::pair<int64, int64> DisplayIdPair;

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

  // The id of the display used as a primary display.
  int64 primary_id;

  // Returns string representation of the layout for debugging/testing.
  std::string ToString() const;
};

}  // namespace ash

#endif
