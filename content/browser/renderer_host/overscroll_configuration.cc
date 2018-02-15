// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/overscroll_configuration.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"

namespace {

bool g_is_mode_initialized = false;
content::OverscrollConfig::Mode g_mode =
    content::OverscrollConfig::Mode::kSimpleUi;

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

// static
OverscrollConfig::Mode OverscrollConfig::GetMode() {
  if (g_is_mode_initialized)
    return g_mode;

  g_is_mode_initialized = true;

  const std::string mode =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kOverscrollHistoryNavigation);
  if (mode == "0")
    g_mode = Mode::kDisabled;
  else if (mode == "1")
    g_mode = Mode::kParallaxUi;

  return g_mode;
}

// static
float OverscrollConfig::GetThreshold(Threshold threshold) {
  switch (threshold) {
    case Threshold::kCompleteTouchpad:
      return kThresholdCompleteTouchpad;

    case Threshold::kCompleteTouchscreen:
      return kThresholdCompleteTouchscreen;

    case Threshold::kStartTouchpad:
      static const float threshold_start_touchpad =
          GetStartThresholdMultiplier() * kThresholdStartTouchpad;
      return threshold_start_touchpad;

    case Threshold::kStartTouchscreen:
      static const float threshold_start_touchscreen =
          GetStartThresholdMultiplier() * kThresholdStartTouchscreen;
      return threshold_start_touchscreen;
  }

  NOTREACHED();
  return -1.f;
}

// static
void OverscrollConfig::SetMode(Mode mode) {
  g_mode = mode;
  g_is_mode_initialized = true;
}

// static
void OverscrollConfig::ResetMode() {
  g_is_mode_initialized = false;
  g_mode = OverscrollConfig::Mode::kSimpleUi;
}

}  // namespace content
