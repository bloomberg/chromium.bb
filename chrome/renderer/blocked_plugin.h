// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BLOCKED_PLUGIN_H_
#define CHROME_RENDERER_BLOCKED_PLUGIN_H_
#pragma once

#include "chrome/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/plugins/npapi/webview_plugin.h"

class GURL;

namespace webkit {
namespace npapi {
class PluginGroup;
}
}

class BlockedPlugin : public RenderViewObserver,
                      public CppBoundClass,
                      public webkit::npapi::WebViewPlugin::Delegate {
 public:
  BlockedPlugin(RenderView* render_view,
                WebKit::WebFrame* frame,
                const webkit::npapi::PluginGroup& info,
                const WebKit::WebPluginParams& params,
                const WebPreferences& settings,
                int template_id,
                const string16& message,
                bool is_blocked_for_prerendering);

  webkit::npapi::WebViewPlugin* plugin() { return plugin_; }

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame);
  virtual void WillDestroyPlugin();
  virtual void ShowContextMenu(const WebKit::WebMouseEvent&);

 private:
  virtual ~BlockedPlugin();

  // RenderViewObserver methods:
  virtual bool OnMessageReceived(const IPC::Message& message);

  void OnMenuItemSelected(unsigned id);

  // Load the blocked plugin.
  void LoadPlugin();

  // Javascript callbacks:
  // Load the blocked plugin by calling LoadPlugin().
  // Takes no arguments, and returns nothing.
  void Load(const CppArgumentList& args, CppVariant* result);

  // Hide the blocked plugin by calling HidePlugin().
  // Takes no arguments, and returns nothing.
  void Hide(const CppArgumentList& args, CppVariant* result);

  // Hide the blocked plugin.
  void HidePlugin();

  WebKit::WebFrame* frame_;
  WebKit::WebPluginParams plugin_params_;
  webkit::npapi::WebViewPlugin* plugin_;
  // The name of the plugin that was blocked.
  string16 name_;
  // True iff the plugin was blocked because the page was being prerendered.
  // Plugin will automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_;
  bool hidden_;
};

#endif  // CHROME_RENDERER_BLOCKED_PLUGIN_H_
