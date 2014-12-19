// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_THROTTLER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_THROTTLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "third_party/WebKit/public/platform/WebRect.h"

namespace blink {
class WebInputEvent;
}

class GURL;
class SkBitmap;

namespace content {

// Manages the Plugin Power Saver feature for a single Pepper plugin instance.
//
// A plugin must meet certain criteria in order to be throttled (e.g. it must
// be a Flash plugin, it must meet certain size criteria, etc.). The process
// for throttling a plugin is as follows:
// 1) Attempt to find a representative keyframe to display as a placeholder for
//    the plugin.
// 2) a) If a representative keyframe is found, throttle the plugin at that
//       keyframe.
//    b) If a representative keyframe is not found, throttle the plugin after a
//       certain period of time.
//
// The plugin will then be unthrottled by receiving a mouse click from the user.
//
// To choose a representative keyframe, we first wait for a certain number of
// "interesting" frames to be displayed by the plugin. A frame is called
// interesting if it meets some heuristic. After we have seen a certain number
// of interesting frames, we throttle the plugin and use that frame as the
// representative keyframe.
class CONTENT_EXPORT PepperPluginInstanceThrottler {
 public:
  // How the throttled power saver is unthrottled, if ever.
  // These numeric values are used in UMA logs; do not change them.
  enum PowerSaverUnthrottleMethod {
    UNTHROTTLE_METHOD_NEVER = 0,
    UNTHROTTLE_METHOD_BY_CLICK = 1,
    UNTHROTTLE_METHOD_BY_WHITELIST = 2,
    UNTHROTTLE_METHOD_BY_AUDIO = 3,
    UNTHROTTLE_METHOD_NUM_ITEMS
  };

  PepperPluginInstanceThrottler(
      RenderFrame* frame,
      const blink::WebRect& bounds,
      const std::string& module_name,
      const GURL& plugin_url,
      RenderFrame::PluginPowerSaverMode power_saver_mode,
      const base::Closure& throttle_change_callback);

  virtual ~PepperPluginInstanceThrottler();

  bool needs_representative_keyframe() const {
    return needs_representative_keyframe_;
  }

  bool power_saver_enabled() const {
    return power_saver_enabled_;
  }

  // Called when the plugin flushes it's graphics context. Supplies the
  // throttler with a candidate to use as the representative keyframe.
  void OnImageFlush(const SkBitmap* bitmap);

  bool is_throttled() const { return plugin_throttled_; }
  const ppapi::ViewData& throttled_view_data() const {
    return empty_view_data_;
  }

  // Returns true if |event| was handled and shouldn't be further processed.
  bool ConsumeInputEvent(const blink::WebInputEvent& event);

  // Disables Power Saver and unthrottles the plugin if already throttled.
  void DisablePowerSaver(PowerSaverUnthrottleMethod method);

 private:
  friend class PepperPluginInstanceThrottlerTest;

  void SetPluginThrottled(bool throttled);

  // Plugin's bounds in view space.
  blink::WebRect bounds_;

  // Called when the throttle state changes.
  base::Closure throttle_change_callback_;

  bool is_flash_plugin_;

  // True if throttler is still waiting to find a representative keyframe.
  bool needs_representative_keyframe_;

  // Number of consecutive interesting frames we've encountered.
  int consecutive_interesting_frames_;

  // Set to true first time plugin is clicked. Used to collect metrics.
  bool has_been_clicked_;

  // Indicates whether this plugin may be throttled to reduce power consumption.
  // |power_saver_enabled_| implies |is_peripheral_content_|.
  bool power_saver_enabled_;

  // Indicates whether this plugin was found to be peripheral content.
  // This is separately tracked from |power_saver_enabled_| to collect UMAs.
  // Always true if |power_saver_enabled_| is true.
  bool is_peripheral_content_;

  // Indicates if the plugin is currently throttled.
  bool plugin_throttled_;

  // Fake view data used by the Power Saver feature to throttle plugins.
  const ppapi::ViewData empty_view_data_;

  base::WeakPtrFactory<PepperPluginInstanceThrottler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PepperPluginInstanceThrottler);
};
}

#endif  // CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_THROTTLER_H_
