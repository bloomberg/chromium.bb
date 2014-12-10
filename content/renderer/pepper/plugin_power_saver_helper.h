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
}

namespace content {

class CONTENT_EXPORT PluginPowerSaverHelper : public RenderFrameObserver {
 public:
  explicit PluginPowerSaverHelper(RenderFrame* render_frame);
  ~PluginPowerSaverHelper() override;

  // See RenderFrame for documentation.
  void RegisterPeripheralPlugin(const GURL& content_origin,
                                const base::Closure& unthrottle_callback);
  bool ShouldThrottleContent(const blink::WebPluginParams& params,
                             const GURL& plugin_frame_url,
                             GURL* poster_image,
                             bool* cross_origin_main_content) const;
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
