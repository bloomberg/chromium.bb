// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_IDS_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_IDS_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

// Palette tools are grouped into different categories. Each tool corresponds to
// exactly one group, and at most one tool can be active per group. Actions are
// actions the user wants to do, such as take a screenshot, and modes generally
// change OS behavior, like showing a laser pointer instead of a cursor.
enum class PaletteGroup { ACTION, MODE };

enum class PaletteToolId {
  NONE,
  CREATE_NOTE,
  CAPTURE_REGION,
  CAPTURE_SCREEN,
  LASER_POINTER,
  MAGNIFY,
};

// Helper functions that convert PaletteToolIds and PaletteGroups to strings.
ASH_EXPORT std::string PaletteToolIdToString(PaletteToolId tool_id);
ASH_EXPORT std::string PaletteGroupToString(PaletteGroup group);

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_IDS_H_
