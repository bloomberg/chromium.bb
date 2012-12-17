// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_IMPL_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_IMPL_H_

#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "googleurl/src/gurl.h"

struct BrowserPluginMsg_UpdateRect_Params;
class WebCursor;

namespace gfx {
class Point;
}

namespace content {

class BrowserPluginManagerImpl : public BrowserPluginManager {
 public:
  BrowserPluginManagerImpl(RenderViewImpl* render_view);

  // BrowserPluginManager implementation.
  virtual BrowserPlugin* CreateBrowserPlugin(
      RenderViewImpl* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params) OVERRIDE;

  // IPC::Sender implementation.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // RenderViewObserver override. Call on render thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
 private:
  virtual ~BrowserPluginManagerImpl();

  void OnPluginAtPositionRequest(const IPC::Message& message,
                                 int request_id,
                                 const gfx::Point& position);

  // Returns whether a message should be forwarded to BrowserPlugins.
  static bool ShouldForwardToBrowserPlugin(const IPC::Message& message);

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginManagerImpl);
};

}  // namespace content

#endif //  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_IMPL_H_
