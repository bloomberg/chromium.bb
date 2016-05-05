// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_SHELL_WINDOW_IDS_H_
#define ASH_WM_COMMON_WM_SHELL_WINDOW_IDS_H_

// Defines the ids assigned to known containers. The id for a WmWindow is
// accessed via GetShellWindowId().

// TODO(sky): move this into wm namespace.
namespace ash {

// NOTE: while the value of these is not necessarily important, the ordering
// is and is enforced in ash/shell_window_ids.h.

// A container used for windows of WINDOW_TYPE_CONTROL that have no parent.
// This container is not visible.
const int kShellWindowId_UnparentedControlContainer = 3;

// The container for standard top-level windows.
const int kShellWindowId_DefaultContainer = 6;

// The container for top-level windows with the 'always-on-top' flag set.
const int kShellWindowId_AlwaysOnTopContainer = 7;

// The container for windows docked to either side of the desktop.
const int kShellWindowId_DockedContainer = 8;

// The container for the shelf.
const int kShellWindowId_ShelfContainer = 9;

// The container for panel windows.
const int kShellWindowId_PanelContainer = 11;

// TODO(sky): this seems unnecessary. Perhaps it should be injected?
// The container for the app list.
const int kShellWindowId_AppListContainer = 12;

// The container for user-specific modal windows.
const int kShellWindowId_SystemModalContainer = 13;

// The container for the lock screen.
const int kShellWindowId_LockScreenContainer = 15;

// The container for the lock screen modal windows.
const int kShellWindowId_LockSystemModalContainer = 16;

// The container for menus.
const int kShellWindowId_MenuContainer = 19;

// The container for drag/drop images and tooltips.
const int kShellWindowId_DragImageAndTooltipContainer = 20;

// ID of the window created by PhantomWindowController or DragWindowController.
const int kShellWindowId_PhantomWindow = 23;

}  // namespace ash

#endif  // ASH_WM_COMMON_WM_SHELL_WINDOW_IDS_H_
