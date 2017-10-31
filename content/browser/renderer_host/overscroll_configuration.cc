// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/overscroll_configuration.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"

namespace {

const float kThresholdCompleteTouchpad = 0.3f;
const float kThresholdCompleteTouchscreen = 0.25f;

const float kThresholdStartTouchpad = 60.f;
const float kThresholdStartTouchscreen = 50.f;

float GetStartThresholdMultiplier() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (!cmd->HasSwitch(switches::kOverscrollStartThreshold))
    return 1.f;

  std::string string_value =
      cmd->GetSwitchValueASCII(switches::kOverscrollStartThreshold);
  int percentage;
  if (base::StringToInt(string_value, &percentage) && percentage > 0)
    return percentage / 100.f;

  DLOG(WARNING) << "Failed to parse switch "
                << switches::kOverscrollStartThreshold << ": " << string_value;
  return 1.f;
}

}  // namespace

namespace content {

float GetOverscrollConfig(OverscrollConfig config) {
  switch (config) {
    case OverscrollConfig::THRESHOLD_COMPLETE_TOUCHPAD:
      return kThresholdCompleteTouchpad;

    case OverscrollConfig::THRESHOLD_COMPLETE_TOUCHSCREEN:
      return kThresholdCompleteTouchscreen;

    case OverscrollConfig::THRESHOLD_START_TOUCHPAD:
      static const float threshold_start_touchpad =
          GetStartThresholdMultiplier() * kThresholdStartTouchpad;
      return threshold_start_touchpad;

    case OverscrollConfig::THRESHOLD_START_TOUCHSCREEN:
      static const float threshold_start_touchscreen =
          GetStartThresholdMultiplier() * kThresholdStartTouchscreen;
      return threshold_start_touchscreen;
  }

  NOTREACHED();
  return -1.f;
}

}  // namespace content
