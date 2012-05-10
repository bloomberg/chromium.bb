// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_NAVIGATOR_H_
#define ASH_LAUNCHER_LAUNCHER_NAVIGATOR_H_
#pragma once

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"

namespace ash {

class LauncherModel;

// Scans the current launcher item and returns the index of the launcher item
// which should be activated next for the specified |direction|.  Returns -1
// if fails to find such item.
ASH_EXPORT int GetNextActivatedItemIndex(const LauncherModel& model,
                                         CycleDirection direction);

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_NAVIGATOR_H_
