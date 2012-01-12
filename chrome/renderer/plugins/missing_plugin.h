// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_MISSING_PLUGIN_H_
#define CHROME_RENDERER_PLUGINS_MISSING_PLUGIN_H_
#pragma once

#include "base/string16.h"
#include "chrome/renderer/plugins/plugin_placeholder.h"
#include "content/public/renderer/render_process_observer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

namespace content {
class RenderThread;
}

class MissingPlugin : public PluginPlaceholder,
                      public content::RenderProcessObserver {
 public:
  // Creates a new WebViewPlugin with a MissingPlugin as a delegate.
  static webkit::WebViewPlugin* Create(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  // WebViewPlugin::Delegate methods:
  virtual void BindWebFrame(WebKit::WebFrame* frame) OVERRIDE;
  virtual void ShowContextMenu(const WebKit::WebMouseEvent&) OVERRIDE;
  virtual void DidFinishLoading() OVERRIDE;

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // IPC::Channel::Listener methods:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
#endif

  // content::RenderViewObserver methods:
  virtual void ContextMenuAction(unsigned id) OVERRIDE;

  // content::RenderProcessObserver methods:
  virtual void PluginListChanged() OVERRIDE;

 private:
  MissingPlugin(content::RenderView* render_view,
                WebKit::WebFrame* frame,
                const WebKit::WebPluginParams& params,
                const std::string& html_data);
  virtual ~MissingPlugin();

  void HideCallback(const CppArgumentList& args, CppVariant* result);

  void OnDidNotFindMissingPlugin();
#if defined(ENABLE_PLUGIN_INSTALLATION)
  void OnFoundMissingPlugin(const string16& plugin_name);
  void OnStartedDownloadingPlugin();
  void OnFinishedDownloadingPlugin();
#endif

  void SetMessage(const string16& message);
  void UpdateMessage();

#if defined(ENABLE_PLUGIN_INSTALLATION)
  // |routing_id()| is the routing ID of our associated RenderView, but we have
  // a separate routing ID for messages specific to this placeholder.
  int32 placeholder_routing_id_;
#endif

  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(MissingPlugin);
};

#endif  // CHROME_RENDERER_PLUGINS_MISSING_PLUGIN_H_
