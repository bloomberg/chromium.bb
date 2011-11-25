// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_PLUGIN_PLACEHOLDER_H_
#pragma once

#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/plugins/webview_plugin.h"

namespace webkit {
struct WebPluginInfo;
}

// Base class to share code between different plug-in placeholders
// used in Chrome. Placeholders can be used if a plug-in is missing or not
// available (blocked or disabled).
class PluginPlaceholder : public content::RenderViewObserver,
                          public CppBoundClass,
                          public webkit::WebViewPlugin::Delegate {
 protected:
  // |render_view| and |frame| are weak pointers. If either one is going away,
  // our |plugin_| will be destroyed as well and will notify us.
  PluginPlaceholder(content::RenderView* render_view,
                    WebKit::WebFrame* frame,
                    const WebKit::WebPluginParams& params,
                    const std::string& html_data);
  virtual ~PluginPlaceholder();

  webkit::WebViewPlugin* plugin() { return plugin_; }
  WebKit::WebFrame* frame() { return frame_; }

  // Can be called by a subclass to replace this placeholder with an actual
  // plugin from |plugin_info|.
  void LoadPluginInternal(const webkit::WebPluginInfo& plugin_info);

  // WebViewPlugin::Delegate methods:
  // Can be called by a subclass to hide this placeholder.
  void HidePluginInternal();

  virtual void BindWebFrame(WebKit::WebFrame* frame) OVERRIDE;
  virtual void WillDestroyPlugin() OVERRIDE;
  virtual void ShowContextMenu(const WebKit::WebMouseEvent& event) OVERRIDE;

 private:
  WebKit::WebFrame* frame_;
  WebKit::WebPluginParams plugin_params_;
  webkit::WebViewPlugin* plugin_;

  DISALLOW_COPY_AND_ASSIGN(PluginPlaceholder);
};

#endif  // CHROME_RENDERER_PLUGINS_PLUGIN_PLACEHOLDER_H_
