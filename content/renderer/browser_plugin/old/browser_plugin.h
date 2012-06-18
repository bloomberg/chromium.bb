// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_H_
#pragma once

#include "base/process.h"
#include "content/renderer/render_view_impl.h"
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
// to a given URL, and establishing a guest-to-embedder channel can take
// hundreds of milliseconds. Furthermore, a RenderView's associated browser-side
// WebContents, RenderViewHost, and SiteInstance must be created and accessed on
// the UI thread of the browser process.
//
// To avoid blocking the embedder RenderView and to avoid introducing the
// potential for deadlock, BrowserPlugin attaches a placeholder that takes
// place of the guest RenderView until the guest has established a connection
// with its embedder RenderView. This permits asynchronously loading of the
// guest while the embedder RenderView is permitted to continue to receive and
// process events.
//
// Furthermore, certain navigations can swap to a new guest RenderView on an
// different process. BrowserPlugin is the consistent facade that the embedder's
// WebKit instance talks to regardless of which process it's communicating with.
class BrowserPlugin {
 public:
  // Creates a new WebViewPlugin with a BrowserPlugin as a delegate.
  static WebKit::WebPlugin* Create(
      RenderViewImpl* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params);

  static BrowserPlugin* FromID(int id);

  webkit::WebViewPlugin* placeholder() { return placeholder_; }

  webkit::ppapi::WebPluginImpl* plugin() { return plugin_; }

  const WebKit::WebPluginParams& plugin_params() const {
    return plugin_params_;
  }

  void LoadGuest(int guest_process_id,
                 const IPC::ChannelHandle& channel_handle);

  void AdvanceFocus(bool reverse);

  RenderViewImpl* render_view() { return render_view_; }

 private:
  BrowserPlugin(RenderViewImpl* render_view,
                WebKit::WebFrame* frame,
                const WebKit::WebPluginParams& params,
                const std::string& html_data);
  virtual ~BrowserPlugin();

  // Parses the source URL of the browser plugin from the element's attributes
  // and outputs them. If not found, it outputs the defaults specified here as
  // parameters.
  void ParseSrcAttribute(const std::string& default_src, std::string* src);
  // Replace the current guest with a new guest.
  void Replace(webkit::ppapi::WebPluginImpl* new_plugin);

  RenderViewImpl* render_view_;
  WebKit::WebPluginParams plugin_params_;
  webkit::WebViewPlugin* placeholder_;
  webkit::ppapi::WebPluginImpl* plugin_;
  int id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPlugin);
};

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_OLD_BROWSER_PLUGIN_H_
