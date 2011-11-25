// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_MISSING_PLUGIN_H_
#define CHROME_RENDERER_PLUGINS_MISSING_PLUGIN_H_
#pragma once

#include "chrome/renderer/plugins/plugin_placeholder.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"

class MissingPlugin : public PluginPlaceholder {
 public:
  // Creates a new WebViewPlugin with a MissingPlugin as a delegate.
  static webkit::WebViewPlugin* Create(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

 private:
  MissingPlugin(content::RenderView* render_view,
                WebKit::WebFrame* frame,
                const WebKit::WebPluginParams& params,
                const std::string& html_data);
  virtual ~MissingPlugin();

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame) OVERRIDE;
  virtual void ShowContextMenu(const WebKit::WebMouseEvent&) OVERRIDE;

  // content::RenderViewObserver methods:
  virtual void ContextMenuAction(unsigned id) OVERRIDE;

  void HideCallback(const CppArgumentList& args, CppVariant* result);

  WebKit::WebString mime_type_;

  DISALLOW_COPY_AND_ASSIGN(MissingPlugin);
};

#endif  // CHROME_RENDERER_PLUGINS_MISSING_PLUGIN_H_
