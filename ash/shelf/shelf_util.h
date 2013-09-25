// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_UTIL_H_
#define ASH_SHELF_SHELF_UTIL_H_

#include "ash/ash_export.h"

namespace ash {

class LauncherModel;

// Return the index of the browser item from a given |launcher_model|.
ASH_EXPORT int GetBrowserItemIndex(const LauncherModel& launcher_model);

}  // namespace ash

#endif  // ASH_SHELF_SHELF_UTIL_H_
