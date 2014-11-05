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

// Print the views::View, ui::Layer and aura::Window hierarchies. This may be
// useful in debugging user reported bugs.
ASH_EXPORT void PrintUIHierarchies();

// Returns true if debug accelerators are enabled.
ASH_EXPORT bool DebugAcceleratorsEnabled();

// Performs |action| and returns true if |action| belongs to a
// debug-only accelerator and debug accelerators are enabled.
ASH_EXPORT bool PerformDebugAction(int action);

}  // namespace debug
}  // namespace ash

#endif  // ASH_ACCELERATORS_DEBUG_COMMANDS_H_
