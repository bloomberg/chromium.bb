// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_
#define CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_

#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_view_observer.h"
#include "ipc/ipc_sender.h"

namespace blink {
class WebFrame;
}

namespace content {

class BrowserPlugin;
class BrowserPluginDelegate;
class RenderViewImpl;

// BrowserPluginManager manages the routing of messages to the appropriate
// BrowserPlugin object based on its instance ID.
class CONTENT_EXPORT BrowserPluginManager
    : public RenderViewObserver,
      public base::RefCounted<BrowserPluginManager> {
 public:
  // Returns the one BrowserPluginManager for this process.
  static BrowserPluginManager* Create(RenderViewImpl* render_view);

  explicit BrowserPluginManager(RenderViewImpl* render_view);

  // Creates a new BrowserPlugin object.
  // BrowserPlugin is responsible for associating itself with the
  // BrowserPluginManager via AddBrowserPlugin. When it is destroyed, it is
  // responsible for removing its association via RemoveBrowserPlugin.
  BrowserPlugin* CreateBrowserPlugin(
      RenderViewImpl* render_view,
      blink::WebFrame* frame,
      scoped_ptr<BrowserPluginDelegate> delegate);

  void Attach(int browser_plugin_instance_id);

  void AddBrowserPlugin(int browser_plugin_instance_id,
                        BrowserPlugin* browser_plugin);
  void RemoveBrowserPlugin(int browser_plugin_instance_id);
  BrowserPlugin* GetBrowserPlugin(int browser_plugin_instance_id) const;

  void UpdateDeviceScaleFactor();
  void UpdateFocusState();
  RenderViewImpl* render_view() const { return render_view_.get(); }
  int GetNextInstanceID();

  // RenderViewObserver override. Call on render thread.
  void DidCommitCompositorFrame() override;
  bool OnMessageReceived(const IPC::Message& message) override;
  bool Send(IPC::Message* msg) override;

  // Don't destroy the BrowserPluginManager when the RenderViewImpl goes away.
  // BrowserPluginManager's lifetime is managed by a reference count. Once
  // the host RenderViewImpl and all BrowserPlugins release their references,
  // then the BrowserPluginManager will be destroyed.
  void OnDestruct() override {}

 private:
  // Friend RefCounted so that the dtor can be non-public.
  friend class base::RefCounted<BrowserPluginManager>;

  ~BrowserPluginManager() override;

  // IPC message handlers.
  void OnCompositorFrameSwappedPluginUnavailable(const IPC::Message& message);

  // This map is keyed by guest instance IDs.
  IDMap<BrowserPlugin> instances_;
  int current_instance_id_;
  base::WeakPtr<RenderViewImpl> render_view_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginManager);
};

}  // namespace content

#endif //  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_MANAGER_H_
