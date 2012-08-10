// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_

#include "base/id_map.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/renderer/render_process_observer.h"
#include "ipc/ipc_sender.h"

class RenderViewImpl;

namespace WebKit {
class WebFrame;
struct WebPluginParams;
}

namespace content {

class BrowserPlugin;

// BrowserPluginManager manages the routing of messages to the appropriate
// BrowserPlugin object based on its instance ID. There is only one
// BrowserPluginManager per renderer process, and it should only be accessed
// by the render thread.
class CONTENT_EXPORT BrowserPluginManager : public IPC::Sender,
                                            public RenderProcessObserver,
                                            public base::NonThreadSafe {
 public:
  // Returns the one BrowserPluginManager for this process.
  static BrowserPluginManager* Get();

  BrowserPluginManager();
  virtual ~BrowserPluginManager();

  // Creates a new BrowserPlugin object with a unique identifier.
  // BrowserPlugin is responsible for associating itself with the
  // BrowserPluginManager via AddBrowserPlugin. When it is destroyed, it is
  // responsible for removing its association via RemoveBrowserPlugin.
  virtual BrowserPlugin* CreateBrowserPlugin(
      RenderViewImpl* render_view,
      WebKit::WebFrame* frame,
      const WebKit::WebPluginParams& params) = 0;

  void AddBrowserPlugin(int instance_id, BrowserPlugin* browser_plugin);
  void RemoveBrowserPlugin(int instance_id);
  BrowserPlugin* GetBrowserPlugin(int instance_id) const;

 protected:
  IDMap<BrowserPlugin> instances_;
  int browser_plugin_counter_;
};

}  // namespace content

#endif //  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_
