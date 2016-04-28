// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_SHELL_WINDOW_IDS_H_
#define ASH_WM_COMMON_WM_SHELL_WINDOW_IDS_H_

#include "ash/ash_export.h"

// Defines the ids assigned to known containers. The id for a WmWindow is
// accessed via GetShellWindowId().

// TODO(sky): move this into wm namespace.
namespace ash {

// NOTE: while the value of these is not necessarily important, the ordering
// is and is enforced in ash/shell_window_ids.h.

// The container for standard top-level windows.
const int kShellWindowId_DefaultContainer = 6;

// The container for top-level windows with the 'always-on-top' flag set.
const int kShellWindowId_AlwaysOnTopContainer = 7;

// The container for windows docked to either side of the desktop.
const int kShellWindowId_DockedContainer = 8;

// The container for panel windows.
const int kShellWindowId_PanelContainer = 11;

// TODO(sky): this seems unnecessary. Perhaps it should be injected?
// The container for the app list.
const int kShellWindowId_AppListContainer = 12;

}  // namespace ash

#endif  // ASH_WM_COMMON_WM_SHELL_WINDOW_IDS_H_
