// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_PLUGIN_POWER_SAVER_HELPER_H_
#define CONTENT_PUBLIC_RENDERER_PLUGIN_POWER_SAVER_HELPER_H_

#include "base/callback_forward.h"
#include "content/common/content_export.h"

class GURL;

namespace blink {
struct WebPluginParams;
}

namespace content {

// PluginPowerSaverHelper manages the plugin content origin whitelist for
// a single RenderFrame.
class CONTENT_EXPORT PluginPowerSaverHelper {
 public:
  virtual ~PluginPowerSaverHelper() {}

  // Returns the absolute URL of the poster image if all the following are true:
  //   a) Power Saver feature is enabled.
  //   b) This plugin instance has a poster image.
  //   c) The content origin and dimensions imply throttling is needed.
  // Returns an empty GURL otherwise.
  virtual GURL GetPluginInstancePosterImage(
      const blink::WebPluginParams& params,
      const GURL& base_url) const = 0;

  // Registers a plugin that has been marked peripheral. If the origin
  // whitelist is later updated and includes |content_origin|, then
  // |unthrottle_callback| will be called.
  virtual void RegisterPeripheralPlugin(
      const GURL& content_origin,
      const base::Closure& unthrottle_callback) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_
