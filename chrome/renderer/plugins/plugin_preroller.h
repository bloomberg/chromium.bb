// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_PLUGIN_PREROLLER_H_
#define CHROME_RENDERER_PLUGINS_PLUGIN_PREROLLER_H_

#include "base/macros.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/renderer/plugin_instance_throttler.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "url/gurl.h"

class SkBitmap;

// This class manages a plugin briefly for the purposes of keyframe extraction.
// Once a keyframe has been extracted, this class will replace the plugin with
// a ChromePluginPlaceholder. The actual plugin will continue to live in a
// throttled state. This class manages its own lifetime.
class PluginPreroller : public content::PluginInstanceThrottler::Observer,
                        public content::RenderFrameObserver {
 public:
  // Does not take ownership of |render_frame|, |plugin|, or |throttler|.
  PluginPreroller(content::RenderFrame* render_frame,
                  const blink::WebPluginParams& params,
                  const content::WebPluginInfo& info,
                  const std::string& identifier,
                  const base::string16& name,
                  const base::string16& message,
                  content::PluginInstanceThrottler* throttler);

  ~PluginPreroller() override;

 private:
  // content::PluginInstanceThrottler::Observer methods:
  void OnKeyframeExtracted(const SkBitmap* bitmap) override;
  void OnThrottleStateChange() override;
  void OnThrottlerDestroyed() override;

  // content::RenderFrameObserver implementation.
  void OnDestruct() override;

  blink::WebPluginParams params_;
  content::WebPluginInfo info_;
  std::string identifier_;
  base::string16 name_;
  base::string16 message_;

  content::PluginInstanceThrottler* throttler_;

  GURL keyframe_data_url_;

  DISALLOW_COPY_AND_ASSIGN(PluginPreroller);
};

#endif  // CHROME_RENDERER_PLUGINS_PLUGIN_PREROLLER_H_
