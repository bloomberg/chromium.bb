// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_BLOCKED_PLUGIN_H_
#define CHROME_RENDERER_PLUGINS_BLOCKED_PLUGIN_H_
#pragma once

#include "chrome/renderer/plugins/plugin_placeholder.h"
#include "webkit/plugins/webplugininfo.h"

namespace webkit {
namespace npapi {
class PluginGroup;
}
}

class BlockedPlugin : public PluginPlaceholder {
 public:
  // Creates a new WebViewPlugin with a BlockedPlugin as a delegate.
  static webkit::WebViewPlugin* Create(content::RenderView* render_view,
                                       WebKit::WebFrame* frame,
                                       const WebKit::WebPluginParams& params,
                                       const webkit::WebPluginInfo& info,
                                       const webkit::npapi::PluginGroup* group,
                                       int resource_id,
                                       int message_id,
                                       bool is_blocked_for_prerendering,
                                       bool allow_loading);

 private:
  BlockedPlugin(content::RenderView* render_view,
                WebKit::WebFrame* frame,
                const WebKit::WebPluginParams& params,
                const std::string& html_data,
                const webkit::WebPluginInfo& info,
                const string16& name,
                bool is_blocked_for_prerendering,
                bool allow_loading);
  virtual ~BlockedPlugin();

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame) OVERRIDE;
  virtual void ShowContextMenu(const WebKit::WebMouseEvent&) OVERRIDE;

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

  webkit::WebPluginInfo plugin_info_;
  // The name of the plugin that was blocked.
  string16 name_;
  // True iff the plugin was blocked because the page was being prerendered.
  // Plugin will automatically be loaded when the page is displayed.
  bool is_blocked_for_prerendering_;
  bool hidden_;
  bool allow_loading_;
};

#endif  // CHROME_RENDERER_PLUGINS_BLOCKED_PLUGIN_H_
