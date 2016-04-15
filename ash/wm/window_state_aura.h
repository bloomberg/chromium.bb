// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_AURA_H_
#define ASH_WM_WINDOW_STATE_AURA_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {
namespace wm {

class WindowState;

// Returns the WindowState for active window. Returns |NULL|
// if there is no active window.
ASH_EXPORT WindowState* GetActiveWindowState();

// Returns the WindowState for |window|. Creates WindowState
// if it didn't exist. The settings object is owned by |window|.
ASH_EXPORT WindowState* GetWindowState(aura::Window* window);

// const version of GetWindowState.
ASH_EXPORT const WindowState* GetWindowState(const aura::Window* window);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_AURA_H_
