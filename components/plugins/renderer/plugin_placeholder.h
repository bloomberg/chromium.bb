// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_
#define COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_

#include "base/memory/weak_ptr.h"
#include "components/plugins/renderer/webview_plugin.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/wrappable.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"

namespace plugins {

class PluginPlaceholder : public content::RenderFrameObserver,
                          public WebViewPlugin::Delegate,
                          public gin::Wrappable<PluginPlaceholder> {
 public:
  static gin::WrapperInfo kWrapperInfo;

  WebViewPlugin* plugin() { return plugin_; }

 protected:
  // |render_frame| and |frame| are weak pointers. If either one is going away,
  // our |plugin_| will be destroyed as well and will notify us.
  PluginPlaceholder(content::RenderFrame* render_frame,
                    blink::WebLocalFrame* frame,
                    const blink::WebPluginParams& params,
                    const std::string& html_data,
                    GURL placeholderDataUrl);

  ~PluginPlaceholder() override;

  blink::WebLocalFrame* GetFrame();
  const blink::WebPluginParams& GetPluginParams() const;

  // WebViewPlugin::Delegate methods:
  void ShowContextMenu(const blink::WebMouseEvent&) override;
  void PluginDestroyed() override;

 private:
  // RenderFrameObserver methods:
  void OnDestruct() override;

  blink::WebLocalFrame* frame_;
  blink::WebPluginParams plugin_params_;
  WebViewPlugin* plugin_;

  DISALLOW_COPY_AND_ASSIGN(PluginPlaceholder);
};

}  // namespace plugins

#endif  // COMPONENTS_PLUGINS_RENDERER_PLUGIN_PLACEHOLDER_H_
