// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_IDS_H_
#define ASH_SHELL_WINDOW_IDS_H_
#pragma once

// Declarations of ids of special shell windows.

namespace ash {

namespace internal {

// The desktop background window.
const int kShellWindowId_DesktopBackgroundContainer = 0;

// The container for standard top-level windows.
const int kShellWindowId_DefaultContainer = 1;

// The container for top-level windows with the 'always-on-top' flag set.
const int kShellWindowId_AlwaysOnTopContainer = 2;

// The container for panel windows.
const int kShellWindowId_PanelContainer = 3;

// The container for the launcher.
const int kShellWindowId_LauncherContainer = 4;

// The container for user-specific modal windows.
const int kShellWindowId_ModalContainer = 5;

// The container for the lock screen.
const int kShellWindowId_LockScreenContainer = 6;

// The container for the lock screen modal windows.
const int kShellWindowId_LockModalContainer = 7;

// The container for the status area.
const int kShellWindowId_StatusContainer = 8;

// The container for menus and tooltips.
const int kShellWindowId_MenuAndTooltipContainer = 9;

// The container for bubbles briefly overlaid onscreen to show settings changes
// (volume, brightness, etc.).
const int kShellWindowId_SettingBubbleContainer = 10;

}  // namespace internal

}  // namespace ash


#endif  // ASH_SHELL_WINDOW_IDS_H_
