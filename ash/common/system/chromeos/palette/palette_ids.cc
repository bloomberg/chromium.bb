// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/palette_ids.h"
#include "base/logging.h"

namespace ash {

std::string PaletteToolIdToString(PaletteToolId tool_id) {
  switch (tool_id) {
    case PaletteToolId::NONE:
      return "NONE";
    case PaletteToolId::CREATE_NOTE:
      return "CREATE_NOTE";
    case PaletteToolId::CAPTURE_REGION:
      return "CAPTURE_REGION";
    case PaletteToolId::CAPTURE_SCREEN:
      return "CAPTURE_SCREEN";
    case PaletteToolId::LASER_POINTER:
      return "LASER_POINTER";
    case PaletteToolId::MAGNIFY:
      return "MAGNIFY";
  }

  NOTREACHED();
  return std::string();
}

std::string PaletteGroupToString(PaletteGroup group) {
  switch (group) {
    case PaletteGroup::ACTION:
      return "ACTION";
    case PaletteGroup::MODE:
      return "MODE";
  }

  NOTREACHED();
  return std::string();
}

}  // namespace ash
