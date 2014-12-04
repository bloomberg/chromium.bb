// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_IMPL_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_IMPL_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/renderer/plugin_power_saver_helper.h"
#include "content/public/renderer/render_frame_observer.h"
#include "url/gurl.h"

namespace content {

class CONTENT_EXPORT PluginPowerSaverHelperImpl : public PluginPowerSaverHelper,
                                                  public RenderFrameObserver {
 public:
  explicit PluginPowerSaverHelperImpl(RenderFrame* render_frame);
  ~PluginPowerSaverHelperImpl() override;

  // PluginPluginPowerSaverHelper implementation.
  GURL GetPluginInstancePosterImage(const blink::WebPluginParams& params,
                                    const GURL& base_url) const override;
  void RegisterPeripheralPlugin(
      const GURL& content_origin,
      const base::Closure& unthrottle_callback) override;

  // Returns true if this plugin should have power saver enabled.
  //
  // Power Saver is enabled for plugin content that are cross-origin and
  // heuristically determined to be not essential to the web page content.
  //
  // Plugin content is defined to be cross-origin when the plugin source's
  // origin differs from the top level frame's origin. For example:
  //  - Cross-origin:  a.com -> b.com/plugin.swf
  //  - Cross-origin:  a.com -> b.com/iframe.html -> b.com/plugin.swf
  //  - Same-origin:   a.com -> b.com/iframe-to-a.html -> a.com/plugin.swf
  //
  // |is_main_attraction| may be NULL. It is set to true if the plugin content
  // is cross-origin and determined to be the "main attraction" of the page.
  //
  // Virtual to allow overriding in tests.
  virtual bool ShouldThrottleContent(const GURL& content_origin,
                                     int width,
                                     int height,
                                     bool* is_main_attraction) const;

  // Virtual to allow overriding in tests.
  virtual void WhitelistContentOrigin(const GURL& content_origin);

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

  DISALLOW_COPY_AND_ASSIGN(PluginPowerSaverHelperImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_IMPL_H_
