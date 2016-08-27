// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/palette/palette_utils.h"

#include "ash/common/ash_switches.h"
#include "base/command_line.h"

namespace ash {

bool IsPaletteFeatureEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnablePalette);
}

bool ArePaletteExperimentalFeaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableExperimentalPaletteFeatures);
}

}  // namespace ash
