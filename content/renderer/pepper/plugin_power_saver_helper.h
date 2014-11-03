// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_

#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "content/public/renderer/render_frame_observer.h"
#include "url/gurl.h"

namespace content {

// PluginPowerSaverWhitelist manages the plugin content origin whitelist for
// a single render frame.
class CONTENT_EXPORT PluginPowerSaverHelper : public RenderFrameObserver {
 public:
  explicit PluginPowerSaverHelper(RenderFrame* render_frame);
  virtual ~PluginPowerSaverHelper();

  // Returns true if this plugin should have power saver enabled.
  //
  // Power Saver is enabled for plugin content that are cross-origin and
  // heuristically determined to be not the "main attraction" of the webpage.
  //
  // Plugin content is defined to be cross-origin when the plugin source's
  // origin differs from the top level frame's origin. For example:
  //  - Cross-origin:  a.com -> b.com/plugin.swf
  //  - Cross-origin:  a.com -> b.com/iframe.html -> b.com/plugin.swf
  //  - Same-origin:   a.com -> b.com/iframe-to-a.html -> a.com/plugin.swf
  //
  // |cross_origin| may not be NULL.
  bool ShouldThrottleContent(const GURL& content_origin,
                             int width,
                             int height,
                             bool* cross_origin) const;

  // Registers a plugin that has been marked peripheral. If the origin
  // whitelist is later updated and includes |content_origin|, then
  // |unthrottle_callback| will be called.
  void RegisterPeripheralPlugin(const GURL& content_origin,
                                const base::Closure& unthrottle_callback);

  void WhitelistContentOrigin(const GURL& content_origin);

 private:
  struct PeripheralPlugin {
    PeripheralPlugin(const GURL& content_origin,
                     const base::Closure& unthrottle_callback);
    ~PeripheralPlugin();

    GURL content_origin;
    base::Closure unthrottle_callback;
  };

  // RenderFrameObserver implementation.
  void DidCommitProvisionalLoad(bool is_new_navigation) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnUpdatePluginContentOriginWhitelist(
      const std::set<GURL>& origin_whitelist);

  // Local copy of the whitelist for the entire tab.
  std::set<GURL> origin_whitelist_;

  // Set of peripheral plugins eligible to be unthrottled ex post facto.
  std::vector<PeripheralPlugin> peripheral_plugins_;

  DISALLOW_COPY_AND_ASSIGN(PluginPowerSaverHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_
