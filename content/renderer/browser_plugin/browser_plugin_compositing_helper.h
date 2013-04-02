// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_COMPOSITING_HELPER_H_
#define  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_COMPOSITING_HELPER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ui/gfx/size.h"

namespace cc {
class CompositorFrame;
class Layer;
class SolidColorLayer;
class TextureLayer;
class DelegatedRendererLayer;
}

namespace WebKit {
class WebPluginContainer;
class WebLayer;
}

namespace content {

class BrowserPluginManager;

class CONTENT_EXPORT BrowserPluginCompositingHelper :
    public base::RefCounted<BrowserPluginCompositingHelper> {
 public:
  BrowserPluginCompositingHelper(WebKit::WebPluginContainer* container,
                                 BrowserPluginManager* manager,
                                 int instance_id,
                                 int host_routing_id);
  void DidCommitCompositorFrame();
  void EnableCompositing(bool);
  void OnContainerDestroy();
  void OnBuffersSwapped(const gfx::Size& size,
                        const std::string& mailbox_name,
                        int gpu_route_id,
                        int gpu_host_id,
                        float device_scale_factor);
  void OnCompositorFrameSwapped(scoped_ptr<cc::CompositorFrame> frame,
                                int route_id,
                                int host_id);
  void UpdateVisibility(bool);
 protected:
  // Friend RefCounted so that the dtor can be non-public.
  friend class base::RefCounted<BrowserPluginCompositingHelper>;
 private:
  ~BrowserPluginCompositingHelper();
  void CheckSizeAndAdjustLayerBounds(const gfx::Size& new_size,
                                     float device_scale_factor,
                                     cc::Layer* layer);
  void FreeMailboxMemory(const std::string& mailbox_name,
                         unsigned sync_point);
  void MailboxReleased(const std::string& mailbox_name,
                       int gpu_route_id,
                       int gpu_host_id,
                       unsigned sync_point);
  int instance_id_;
  int host_routing_id_;
  int last_route_id_;
  int last_host_id_;
  bool last_mailbox_valid_;
  bool ack_pending_;
  bool ack_pending_for_crashed_guest_;

  gfx::Size buffer_size_;

  scoped_refptr<cc::SolidColorLayer> background_layer_;
  scoped_refptr<cc::TextureLayer> texture_layer_;
  scoped_refptr<cc::DelegatedRendererLayer> delegated_layer_;
  scoped_ptr<WebKit::WebLayer> web_layer_;
  WebKit::WebPluginContainer* container_;

  scoped_refptr<BrowserPluginManager> browser_plugin_manager_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_COMPOSITING_HELPER_H_
