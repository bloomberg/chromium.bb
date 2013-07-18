// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_COMPOSITING_HELPER_H_
#define  CONTENT_RENDERER_BROWSER_PLUGIN_BROWSER_PLUGIN_COMPOSITING_HELPER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/gfx/size.h"

namespace base {
class SharedMemory;
}

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
                                uint32 output_surface_id,
                                int host_id);
  void UpdateVisibility(bool);
 protected:
  // Friend RefCounted so that the dtor can be non-public.
  friend class base::RefCounted<BrowserPluginCompositingHelper>;
 private:
  enum SwapBuffersType {
    TEXTURE_IMAGE_TRANSPORT,
    GL_COMPOSITOR_FRAME,
    SOFTWARE_COMPOSITOR_FRAME,
  };
  struct SwapBuffersInfo {
    SwapBuffersInfo();

    gpu::Mailbox name;
    SwapBuffersType type;
    gfx::Size size;
    int route_id;
    uint32 output_surface_id;
    int host_id;
    unsigned software_frame_id;
    base::SharedMemory* shared_memory;
  };
  ~BrowserPluginCompositingHelper();
  void CheckSizeAndAdjustLayerBounds(const gfx::Size& new_size,
                                     float device_scale_factor,
                                     cc::Layer* layer);
  void OnBuffersSwappedPrivate(const SwapBuffersInfo& mailbox,
                               unsigned sync_point,
                               float device_scale_factor);
  void MailboxReleased(SwapBuffersInfo mailbox,
                       unsigned sync_point,
                       bool lost_resource);
  int instance_id_;
  int host_routing_id_;
  int last_route_id_;
  uint32 last_output_surface_id_;
  int last_host_id_;
  bool last_mailbox_valid_;
  bool ack_pending_;

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
