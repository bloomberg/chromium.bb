// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_PLACEHOLDER_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_PLACEHOLDER_H_
#pragma once

#include "base/process.h"
#include "ipc/ipc_channel_handle.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "ui/gfx/size.h"
#include "webkit/plugins/webview_plugin.h"

namespace content {
class RenderView;
}

namespace WebKit {
class WebPlugin;
}

// A browser plugin is a plugin container that hosts an out-of-process "guest"
// RenderView. Loading up a new process, creating a new RenderView, navigating
// to a given URL, and establishing a guest-to-host channel can take hundreds
// of milliseconds. Furthermore, a RenderView's associated browser-side
// WebContents, RenderViewHost, and SiteInstance must be created and accessed on
// the UI thread of the browser process.
//
// To avoid blocking the host RenderView and to avoid introducing the potential
// for deadlock, the BrowserPluginPlaceholder takes place of the guest
// RenderView until the guest has established a connection with its host
// RenderView. This permits loading the guest to happen asynchronously, while
// the host RenderView is permitted to continue to receive and process events.
class BrowserPluginPlaceholder: public webkit::WebViewPlugin::Delegate {
 public:
  // Creates a new WebViewPlugin with a BrowserPluginPlaceholder as a delegate.
  static webkit::WebViewPlugin* Create(
      content::RenderView* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  static BrowserPluginPlaceholder* FromID(int id);

  void RegisterPlaceholder(int id, BrowserPluginPlaceholder* placeholder);
  void UnregisterPlaceholder(int id);

  int GetID() { return id_; }

  webkit::WebViewPlugin* plugin() { return plugin_; }

  const WebKit::WebPluginParams& plugin_params() const;

  void GuestReady(base::ProcessHandle process_handle,
                  const IPC::ChannelHandle& channel_handle);

  content::RenderView* render_view() { return render_view_; }

 private:
  BrowserPluginPlaceholder(content::RenderView* render_view,
                           WebKit::WebFrame* frame,
                           const WebKit::WebPluginParams& params,
                           const std::string& html_data);
  virtual ~BrowserPluginPlaceholder();

  // Grabs the width, height, and source URL of the browser plugin
  // from the element's attributes. If not found, it uses the defaults
  // specified here as parameters.
  void GetPluginParameters(int default_width, int default_height,
                           const std::string& default_src);
  // Replace this placeholder with the real browser plugin.
  void LoadGuest(WebKit::WebPlugin* new_plugin);

  virtual void BindWebFrame(WebKit::WebFrame* frame) OVERRIDE { }
  virtual void WillDestroyPlugin() OVERRIDE;
  virtual void ShowContextMenu(const WebKit::WebMouseEvent&) OVERRIDE { }

  content::RenderView* render_view_;
  WebKit::WebPluginParams plugin_params_;
  webkit::WebViewPlugin* plugin_;
  int id_;
  gfx::Size size_;
  std::string src_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginPlaceholder);
};

#endif  // CONTNET_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_PLACEHOLDER_H_
