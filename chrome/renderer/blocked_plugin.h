// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_BLOCKED_PLUGIN_H_
#define CHROME_RENDERER_BLOCKED_PLUGIN_H_
#pragma once

#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginParams.h"
#include "webkit/glue/cpp_bound_class.h"
#include "webkit/glue/plugins/webview_plugin.h"

class GURL;
class PluginGroup;
class RenderView;

class BlockedPlugin : public CppBoundClass,
                      public WebViewPlugin::Delegate,
                      public NotificationObserver {
 public:
  BlockedPlugin(RenderView* render_view,
                WebKit::WebFrame* frame,
                const WebKit::WebPluginParams& params,
                const WebPreferences& settings,
                PluginGroup* group);

  WebViewPlugin* plugin() { return plugin_; }

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame);
  virtual void WillDestroyPlugin();

  // NotificationObserver methods:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  virtual ~BlockedPlugin();

  // Javascript callbacks:
  // Load the blocked plugin by calling LoadPlugin() below.
  // Takes no arguments, and returns nothing.
  void Load(const CppArgumentList& args, CppVariant* result);

  // Load the blocked plugin.
  void LoadPlugin();

  RenderView* render_view_;
  WebKit::WebFrame* frame_;
  WebKit::WebPluginParams plugin_params_;
  WebViewPlugin* plugin_;

  NotificationRegistrar registrar_;
};

#endif  // CHROME_RENDERER_BLOCKED_PLUGIN_H_
