// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BLOCKED_PLUGIN_H_
#define CHROME_RENDERER_BLOCKED_PLUGIN_H_
#pragma once

#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/renderer/custom_menu_listener.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginParams.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/plugins/npapi/webview_plugin.h"

class GURL;
class RenderView;


namespace webkit {
namespace npapi {
class PluginGroup;
}
}

class BlockedPlugin : public CppBoundClass,
                      public webkit::npapi::WebViewPlugin::Delegate,
                      public NotificationObserver,
                      public CustomMenuListener {
 public:
  BlockedPlugin(RenderView* render_view,
                WebKit::WebFrame* frame,
                const webkit::npapi::PluginGroup& info,
                const WebKit::WebPluginParams& params,
                const WebPreferences& settings,
                int template_id,
                const string16& message);

  webkit::npapi::WebViewPlugin* plugin() { return plugin_; }

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame);
  virtual void WillDestroyPlugin();
  virtual void ShowContextMenu(const WebKit::WebMouseEvent&);

  // NotificationObserver methods:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // CustomMenuListener methods:
  virtual void MenuItemSelected(unsigned id);

 private:
  virtual ~BlockedPlugin();

  // Javascript callbacks:
  // Load the blocked plugin by calling LoadPlugin() below.
  // Takes no arguments, and returns nothing.
  void Load(const CppArgumentList& args, CppVariant* result);

  // Load the blocked plugin.
  void LoadPlugin();

  // Hide the blocked plugin.
  void HidePlugin();

  RenderView* render_view_;
  WebKit::WebFrame* frame_;
  WebKit::WebPluginParams plugin_params_;
  webkit::npapi::WebViewPlugin* plugin_;
  // The name of the plugin that was blocked.
  string16 name_;

  NotificationRegistrar registrar_;
};

#endif  // CHROME_RENDERER_BLOCKED_PLUGIN_H_
