// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_THROTTLER_H_
#define CONTENT_RENDERER_PEPPER_PEPPER_PLUGIN_INSTANCE_THROTTLER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
#include "third_party/WebKit/public/platform/WebRect.h"

namespace blink {
class WebInputEvent;
}

class GURL;

namespace content {

class PluginPowerSaverHelper;

// Manages the Plugin Power Saver feature for a single Pepper plugin instance.
class CONTENT_EXPORT PepperPluginInstanceThrottler {
 public:
  PepperPluginInstanceThrottler(PluginPowerSaverHelper* power_saver_helper,
                                const blink::WebRect& bounds,
                                const std::string& module_name,
                                const GURL& plugin_url,
                                const base::Closure& throttle_change_callback);

  virtual ~PepperPluginInstanceThrottler();

  bool is_throttled() const { return plugin_throttled_; }
  const ppapi::ViewData& throttled_view_data() const {
    return empty_view_data_;
  }

  // Returns true if |event| was handled and shouldn't be further processed.
  bool ConsumeInputEvent(const blink::WebInputEvent& event);

 private:
  friend class PepperPluginInstanceThrottlerTest;

  void SetPluginThrottled(bool throttled);
  void DisablePowerSaverByRetroactiveWhitelist();

  // Plugin's bounds in view space.
  blink::WebRect bounds_;

  // Called when the throttle state changes.
  base::Closure throttle_change_callback_;

  bool is_flash_plugin_;

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
