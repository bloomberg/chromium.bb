// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_COMMANDS_CLASSIC_H_
#define ASH_ACCELERATORS_ACCELERATOR_COMMANDS_CLASSIC_H_

#include "ash/ash_export.h"

// This file contains implementations of commands that are bound to keyboard
// shortcuts in Ash or in the embedding application (e.g. Chrome).
namespace ash {
namespace accelerators {

// Toggles touch HUD.
ASH_EXPORT void ToggleTouchHudProjection();

// If it is in the pinned mode, exit from it.
ASH_EXPORT void Unpin();

}  // namespace accelerators
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_COMMANDS_CLASSIC_H_
