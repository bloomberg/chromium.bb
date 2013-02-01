// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_UTIL_H_
#define ASH_LAUNCHER_LAUNCHER_UTIL_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ui {
class Event;
}

namespace ash {
class LauncherModel;

namespace launcher {

// Return the index of the browser item from a given |launcher_model|.
ASH_EXPORT int GetBrowserItemIndex(const LauncherModel& launcher_model);

// Move the |maybe_panel| to the root window where the |event| occured if
// |maybe_panel| is of aura::client::WINDOW_TYPE_PANEL and it's not
// in the same root window.
ASH_EXPORT void MoveToEventRootIfPanel(aura::Window* maybe_panel,
                                       const ui::Event& event);

}  // namespace launcher
}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_UTIL_H_
