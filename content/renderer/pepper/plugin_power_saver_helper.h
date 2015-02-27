// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_
#define CONTENT_RENDERER_PEPPER_PLUGIN_POWER_SAVER_HELPER_H_

#include <set>
#include <vector>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/renderer/render_frame_observer.h"
#include "url/gurl.h"

namespace blink {
struct WebPluginParams;
struct WebRect;
}

namespace content {

class CONTENT_EXPORT PluginPowerSaverHelper : public RenderFrameObserver {
 public:
  explicit PluginPowerSaverHelper(RenderFrame* render_frame);
  ~PluginPowerSaverHelper() override;

  // See RenderFrame for documentation.
  void RegisterPeripheralPlugin(const GURL& content_origin,
                                const base::Closure& unthrottle_callback);

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
  // |page_frame_url| is the URL of the frame containing the plugin, which may
  // be different from the URL of the top level document.
  //
  // |poster_image| may be NULL. It is set to the absolute URL of the poster
  // image if it exists and this method returns true. Otherwise, an empty GURL.
  //
  // |cross_origin_main_content| may be NULL. It is set to true if the
  // plugin content is cross-origin but still the "main attraction" of the page.
  bool ShouldThrottleContent(const GURL& content_origin,
                             const std::string& plugin_module_name,
                             int width,
                             int height,
                             bool* cross_origin_main_content) const;

  // Whitelists a |content_origin| so its content will never be throttled in
  // this RenderFrame. Whitelist is cleared by top level navigation.
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
  void DidCommitProvisionalLoad(bool is_new_navigation,
                                bool is_same_page_navigation) override;
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
