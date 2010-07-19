// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BLOCKED_PLUGIN_H_
#define CHROME_RENDERER_BLOCKED_PLUGIN_H_

#include "third_party/WebKit/WebKit/chromium/public/WebPluginParams.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/plugins/webview_plugin.h"

class RenderView;

class BlockedPlugin : public CppBoundClass,
                      public WebViewPlugin::Delegate {
 public:
  BlockedPlugin(RenderView* render_view,
                WebKit::WebFrame* frame,
                const WebKit::WebPluginParams& params);

  void Load(const CppArgumentList& args, CppVariant* result);
  void LoadPlugin();

  WebViewPlugin* plugin() { return plugin_; }

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame);
  virtual void WillDestroyPlugin();

 private:
  virtual ~BlockedPlugin() { }

  RenderView* render_view_;
  WebKit::WebFrame* frame_;
  WebKit::WebPluginParams plugin_params_;
  WebViewPlugin* plugin_;
};

#endif  // CHROME_RENDERER_BLOCKED_PLUGIN_H_
