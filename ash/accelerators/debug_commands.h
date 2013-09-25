// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_DEBUG_COMMANDS_H_
#define ASH_ACCELERATORS_DEBUG_COMMANDS_H_

#include "ash/ash_export.h"

// This file contains implementations of commands that are used only
// when running on desktop for debugging.
namespace ash {
namespace debug {

// Cycle through different wallpaper modes. This is used when running
// on desktop for testing.
ASH_EXPORT bool CycleDesktopBackgroundMode();

}  // namespace debug
}  // namespace ash

#endif  // ASH_ACCELERATORS_DEBUG_COMMANDS_H_
