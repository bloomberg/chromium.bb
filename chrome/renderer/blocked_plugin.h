// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BLOCKED_PLUGIN_H_
#define CHROME_RENDERER_BLOCKED_PLUGIN_H_
#pragma once

#include "content/public/renderer/render_view_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/plugins/npapi/webview_plugin.h"
#include "webkit/plugins/webplugininfo.h"

class BlockedPlugin : public content::RenderViewObserver,
                      public CppBoundClass,
                      public webkit::npapi::WebViewPlugin::Delegate {
 public:
  BlockedPlugin(content::RenderView* render_view,
                WebKit::WebFrame* frame,
                const webkit::WebPluginInfo& info,
                const WebKit::WebPluginParams& params,
                const WebPreferences& settings,
                int template_id,
                const string16& name,
                const string16& message,
                bool is_blocked_for_prerendering,
                bool allow_loading);

  webkit::npapi::WebViewPlugin* plugin() { return plugin_; }

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame) OVERRIDE;
  virtual void WillDestroyPlugin() OVERRIDE;
  virtual void ShowContextMenu(const WebKit::WebMouseEvent&) OVERRIDE;

 private:
  virtual ~BlockedPlugin();

  // RenderViewObserver methods:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void ContextMenuAction(unsigned id) OVERRIDE;

  void OnLoadBlockedPlugins();
  void OnSetIsPrerendering(bool is_prerendering);

  // Javascript callbacks:
  // Load the blocked plugin by calling LoadPlugin().
  // Takes no arguments, and returns nothing.
  void LoadCallback(const CppArgumentList& args, CppVariant* result);

  // Hide the blocked plugin by calling HidePlugin().
  // Takes no arguments, and returns nothing.
  void HideCallback(const CppArgumentList& args, CppVariant* result);

  // Opens a URL in a new tab.
  // Takes one argument, a string specifying the URL to open. Returns nothing.
  void OpenUrlCallback(const CppArgumentList& args, CppVariant* result);

  // Load the blocked plugin.
  void LoadPlugin();

  // Hide the blocked plugin.
  void HidePlugin();

  WebKit::WebFrame* frame_;
  webkit::WebPluginInfo plugin_info_;
  WebKit::WebPluginParams plugin_params_;
  webkit::npapi::WebViewPlugin* plugin_;
  // The name of the plugin that was blocked.
  string16 name_;
  // True iff the plugin was blocked because the page was being prerendered.
  // Plugin will automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_;
  bool hidden_;
  bool allow_loading_;
};

#endif  // CHROME_RENDERER_BLOCKED_PLUGIN_H_
