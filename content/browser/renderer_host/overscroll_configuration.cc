// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/overscroll_configuration.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_switches.h"

namespace {

float g_horiz_threshold_complete = 0.25f;
float g_vert_threshold_complete = 0.20f;

float g_horiz_threshold_start_touchscreen = 50.f;
float g_horiz_threshold_start_touchpad = 50.f;
float g_vert_threshold_start = 0.f;

float g_horiz_resist_after = 30.f;
float g_vert_resist_after = 30.f;

float GetHorizontalStartThresholdMultiplier() {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (!cmd->HasSwitch(switches::kOverscrollStartThreshold))
    return 1.f;
  std::string string_value =
      cmd->GetSwitchValueASCII(switches::kOverscrollStartThreshold);
  int percentage;
  if (base::StringToInt(string_value, &percentage) && percentage > 0) {
    return percentage / 100.f;
  }

  DLOG(WARNING) << "Failed to parse switch "
               << switches::kOverscrollStartThreshold << ": " << string_value;
  return 1.f;
}

}  // namespace

namespace content {

float GetOverscrollConfig(OverscrollConfig config) {
  switch (config) {
    case OVERSCROLL_CONFIG_HORIZ_THRESHOLD_COMPLETE:
      return g_horiz_threshold_complete;

    case OVERSCROLL_CONFIG_VERT_THRESHOLD_COMPLETE:
      return g_vert_threshold_complete;

    case OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHSCREEN:
      static const float horiz_threshold_start_touchscreen =
          GetHorizontalStartThresholdMultiplier() *
          g_horiz_threshold_start_touchscreen;
      return horiz_threshold_start_touchscreen;

    case OVERSCROLL_CONFIG_HORIZ_THRESHOLD_START_TOUCHPAD:
      static const float horiz_threshold_start_touchpad =
          GetHorizontalStartThresholdMultiplier() *
          g_horiz_threshold_start_touchpad;
      return horiz_threshold_start_touchpad;

    case OVERSCROLL_CONFIG_VERT_THRESHOLD_START:
      return g_vert_threshold_start;

    case OVERSCROLL_CONFIG_HORIZ_RESIST_AFTER:
      return g_horiz_resist_after;

    case OVERSCROLL_CONFIG_VERT_RESIST_AFTER:
      return g_vert_resist_after;

    case OVERSCROLL_CONFIG_NONE:
    case OVERSCROLL_CONFIG_COUNT:
      NOTREACHED();
  }

  return -1.f;
}

}  // namespace content
