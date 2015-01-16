// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/plugin_instance_throttler.h"

#include "content/renderer/pepper/plugin_instance_throttler_impl.h"

namespace content {

// static
scoped_ptr<PluginInstanceThrottler> PluginInstanceThrottler::Get(
    RenderFrame* frame,
    const GURL& plugin_url,
    PluginPowerSaverMode power_saver_mode) {
  if (power_saver_mode == PluginPowerSaverMode::POWER_SAVER_MODE_ESSENTIAL)
    return nullptr;

  bool power_saver_enabled =
      power_saver_mode ==
      PluginPowerSaverMode::POWER_SAVER_MODE_PERIPHERAL_THROTTLED;
  return make_scoped_ptr(
      new PluginInstanceThrottlerImpl(frame, plugin_url, power_saver_enabled));
}

}  // namespace content
