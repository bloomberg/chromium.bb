// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_MOVE_WINDOW_UTIL_H_
#define ASH_DISPLAY_DISPLAY_MOVE_WINDOW_UTIL_H_

#include "ash/ash_export.h"

namespace ash {

enum class DisplayMoveWindowDirection { kAbove, kBelow, kLeft, kRight };

// Returns true if active window can be moved between displays by accelerators.
ASH_EXPORT bool CanHandleMoveActiveWindowBetweenDisplays();

// Handles moving current active window from its display to another display
// specified by |Direction|.
ASH_EXPORT void HandleMoveActiveWindowToDisplay(
    DisplayMoveWindowDirection direction);

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_MOVE_WINDOW_UTIL_H_
