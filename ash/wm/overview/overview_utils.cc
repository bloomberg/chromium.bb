// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_utils.h"

#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/optional.h"

namespace ash {

namespace {

// Cache the result of the command line lookup so the command line is only
// looked up once.
base::Optional<bool> g_enable_new_overview_ui = base::nullopt;

}  // namespace

bool IsNewOverviewUi() {
  if (g_enable_new_overview_ui == base::nullopt) {
    g_enable_new_overview_ui =
        base::make_optional(base::CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kAshEnableNewOverviewUi));
  }

  return g_enable_new_overview_ui.value();
}

void ResetCachedOverviewUiValueForTesting() {
  g_enable_new_overview_ui = base::nullopt;
}

}  // namespace ash
