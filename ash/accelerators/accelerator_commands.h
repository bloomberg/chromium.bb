// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
#define ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_

#include "ash/ash_export.h"

// This file contains implementations of commands that are bound to keyboard
// shortcuts in Ash or in the embedding application (e.g. Chrome).
namespace ash {
namespace accelerators {

// Minimizes the active window, if present. If no windows are active, restores
// the first unminimized window. Returns true if a window was minimized or
// restored.
ASH_EXPORT bool ToggleMinimized();

// Toggles the maxmized state. If the window is in fulllscreen, it exits
// fullscreen mode.
ASH_EXPORT void ToggleMaximized();

// Toggles the fullscreen state. The behavior can be overridden
// by WindowStateDelegate::ToggleFullscreen().
ASH_EXPORT void ToggleFullscreen();

// Toggles touch HUD.
ASH_EXPORT void ToggleTouchHudProjection();

// True if the desktop zoom accelerator is enabled.
bool IsInternalDisplayZoomEnabled();

// Change internal display zooming up or down.
ASH_EXPORT bool ZoomInternalDisplay(bool up);

// Reset the display zooming to the default state.
ASH_EXPORT void ResetInternalDisplayZoom();

}  // namespace accelerators
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_COMMANDS_H_
