// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/input_router_config_helper.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/gesture_detection/gesture_detector.h"

namespace content {
namespace {

// Default time allowance for the touch ack delay before the touch sequence is
// cancelled, depending on whether the site has a mobile-friendly viewport.
// Note that these constants are effective only when the timeout is supported.
const int kDesktopTouchAckTimeoutDelayMs = 200;
const int kMobileTouchAckTimeoutDelayMs = 1000;

PassthroughTouchEventQueue::Config GetTouchEventQueueConfig() {
  PassthroughTouchEventQueue::Config config;

  config.desktop_touch_ack_timeout_delay =
      base::TimeDelta::FromMilliseconds(kDesktopTouchAckTimeoutDelayMs);
  config.mobile_touch_ack_timeout_delay =
      base::TimeDelta::FromMilliseconds(kMobileTouchAckTimeoutDelayMs);

#if defined(OS_ANDROID)
  // For historical reasons only Android enables the touch ack timeout.
  config.touch_ack_timeout_supported = true;
#else
  config.touch_ack_timeout_supported = false;
#endif

  return config;
}

GestureEventQueue::Config GetGestureEventQueueConfig() {
  GestureEventQueue::Config config;
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  config.debounce_interval = base::TimeDelta::FromMilliseconds(
      gesture_config->scroll_debounce_interval_in_ms());

  config.fling_config.touchscreen_tap_suppression_config.enabled =
      gesture_config->fling_touchscreen_tap_suppression_enabled();
  config.fling_config.touchscreen_tap_suppression_config
      .max_cancel_to_down_time = base::TimeDelta::FromMilliseconds(
      gesture_config->fling_max_cancel_to_down_time_in_ms());

  config.fling_config.touchpad_tap_suppression_config.enabled =
      gesture_config->fling_touchpad_tap_suppression_enabled();
  config.fling_config.touchpad_tap_suppression_config.max_cancel_to_down_time =
      base::TimeDelta::FromMilliseconds(
          gesture_config->fling_max_cancel_to_down_time_in_ms());

  return config;
}

}  // namespace

InputRouter::Config::Config() {}

InputRouter::Config GetInputRouterConfigForPlatform() {
  InputRouter::Config config;
  config.gesture_config = GetGestureEventQueueConfig();
  config.touch_config = GetTouchEventQueueConfig();
  return config;
}

}  // namespace content
