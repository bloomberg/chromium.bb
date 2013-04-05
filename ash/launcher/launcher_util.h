// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_UTIL_H_
#define ASH_LAUNCHER_LAUNCHER_UTIL_H_

#include "ash/ash_export.h"

namespace ash {
class LauncherModel;

namespace launcher {

// Return the index of the browser item from a given |launcher_model|.
ASH_EXPORT int GetBrowserItemIndex(const LauncherModel& launcher_model);

}  // namespace launcher
}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_UTIL_H_
