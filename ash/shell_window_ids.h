// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_IDS_H_
#define ASH_SHELL_WINDOW_IDS_H_

// Declarations of ids of special shell windows.

namespace ash {

// TODO: we're using this in random places outside of ash, it shouldn't be in
// internal.
namespace internal {

// A higher-level container that holds all of the containers stacked below
// kShellWindowId_LockScreenContainer.  Only used by PowerButtonController for
// animating lower-level containers.
const int kShellWindowId_NonLockScreenContainersContainer = 0;

// A higher-level container that holds containers that hold lock-screen
// windows.  Only used by PowerButtonController for animating lower-level
// containers.
const int kShellWindowId_LockScreenContainersContainer = 1;

// A higher-level container that holds containers that hold lock-screen-related
// windows (which we want to display while the screen is locked; effectively
// containers stacked above kShellWindowId_LockSystemModalContainer).  Only used
// by PowerButtonController for animating lower-level containers.
const int kShellWindowId_LockScreenRelatedContainersContainer = 2;

// A container used for windows of WINDOW_TYPE_CONTROL that have no parent.
// This container is not visible.
const int kShellWindowId_UnparentedControlContainer = 3;

// The desktop background window.
const int kShellWindowId_DesktopBackgroundContainer = 4;

// TODO(sky): rename kShellWindowId_DefaultContainer.

// The container for standard top-level windows.
// WARNING: the only children of kShellWindowId_DefaultContainer are
// kShellWindowId_WorkspaceContainer.
const int kShellWindowId_DefaultContainer = 5;

// Used by Worskpace2 for each workspace. Contains standard top-level windows.
// WARNING: there may be more than one container with this id.
const int kShellWindowId_WorkspaceContainer = 6;

// The container for top-level windows with the 'always-on-top' flag set.
const int kShellWindowId_AlwaysOnTopContainer = 7;

// The container for panel windows.
const int kShellWindowId_PanelContainer = 8;

// The container for the launcher.
const int kShellWindowId_LauncherContainer = 9;

// The container for the app list.
const int kShellWindowId_AppListContainer = 10;

// The container for user-specific modal windows.
const int kShellWindowId_SystemModalContainer = 11;

// The container for input method components such like candidate windows.  They
// are almost panels but have no activations/focus, and they should appear over
// the AppList and SystemModal dialogs.
const int kShellWindowId_InputMethodContainer = 12;

// The container for the lock screen background.
const int kShellWindowId_LockScreenBackgroundContainer = 13;

// The container for the lock screen.
const int kShellWindowId_LockScreenContainer = 14;

// The container for the lock screen modal windows.
const int kShellWindowId_LockSystemModalContainer = 15;

// The container for the status area.
const int kShellWindowId_StatusContainer = 16;

// The container for menus.
const int kShellWindowId_MenuContainer = 17;

// The container for drag/drop images and tooltips.
const int kShellWindowId_DragImageAndTooltipContainer = 18;

// The container for bubbles briefly overlaid onscreen to show settings changes
// (volume, brightness, etc.).
const int kShellWindowId_SettingBubbleContainer = 19;

// The container for special components overlaid onscreen, such as the
// region selector for partial screenshots.
const int kShellWindowId_OverlayContainer = 20;

// ID of the window created by PhantomWindowController or DragWindowController.
const int kShellWindowId_PhantomWindow = 21;

// The topmost container, used for power off animation.
const int kShellWindowId_PowerButtonAnimationContainer = 22;

}  // namespace internal

}  // namespace ash


#endif  // ASH_SHELL_WINDOW_IDS_H_
