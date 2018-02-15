// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_utils.h"

#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

// Cache the result of the command line lookup so the command line is only
// looked up once.
base::Optional<bool> g_enable_new_overview_animations = base::nullopt;
base::Optional<bool> g_enable_new_overview_ui = base::nullopt;

}  // namespace

bool CanCoverAvailableWorkspace(aura::Window* window) {
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (split_view_controller->IsSplitViewModeActive())
    return split_view_controller->CanSnap(window);
  return wm::GetWindowState(window)->IsMaximizedOrFullscreenOrPinned();
}

bool IsNewOverviewAnimationsEnabled() {
  if (g_enable_new_overview_animations == base::nullopt) {
    g_enable_new_overview_animations =
        base::make_optional(base::CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kAshEnableNewOverviewAnimations));
  }

  return g_enable_new_overview_animations.value();
}

bool IsNewOverviewUi() {
  if (g_enable_new_overview_ui == base::nullopt) {
    g_enable_new_overview_ui =
        base::make_optional(base::CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kAshEnableNewOverviewUi));
  }

  return g_enable_new_overview_ui.value();
}

void ResetCachedOverviewAnimationsValueForTesting() {
  g_enable_new_overview_animations = base::nullopt;
}

void ResetCachedOverviewUiValueForTesting() {
  g_enable_new_overview_ui = base::nullopt;
}

}  // namespace ash
