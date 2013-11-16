// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_UTIL_H_
#define ASH_SHELF_SHELF_UTIL_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"

namespace aura {
class Window;
}

namespace ash {

// Associates LauncherItem of |id| with specified |window|.
ASH_EXPORT void SetLauncherIDForWindow(LauncherID id, aura::Window* window);

// Returns the id of the LauncherItem associated with the specified |window|,
// or 0 if there isn't one.
// Note: Window of a tabbed browser will return the |LauncherID| of the
// currently active tab.
ASH_EXPORT LauncherID GetLauncherIDForWindow(aura::Window* window);

}  // namespace ash

#endif  // ASH_SHELF_SHELF_UTIL_H_
