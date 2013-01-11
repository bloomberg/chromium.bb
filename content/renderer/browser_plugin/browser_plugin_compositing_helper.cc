// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_compositing_helper.h"

#include "cc/texture_layer.h"
#include "content/common/browser_plugin_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

namespace content {

static void SendACK(const std::string& mailbox_name,
                    int host_route_id,
                    int gpu_route_id,
                    int gpu_host_id,
                    unsigned sync_point) {
  RenderThread::Get()->Send(
      new BrowserPluginHostMsg_BuffersSwappedACK(
          host_route_id,
          gpu_route_id,
          gpu_host_id,
          mailbox_name,
          sync_point));
}

BrowserPluginCompositingHelper::BrowserPluginCompositingHelper(
    WebKit::WebPluginContainer* container,
    int host_routing_id)
    : host_routing_id_(host_routing_id),
      last_mailbox_valid_(false),
      container_(container) {
}

BrowserPluginCompositingHelper::~BrowserPluginCompositingHelper() {
  container_->setWebLayer(NULL);
}

void BrowserPluginCompositingHelper::EnableCompositing(bool enable) {
  if (enable && !texture_layer_) {
    texture_layer_ = cc::TextureLayer::createForMailbox();
    web_layer_.reset(new WebKit::WebLayerImpl(texture_layer_));
  }

  container_->setWebLayer(enable ? web_layer_.get() : NULL);
}

void BrowserPluginCompositingHelper::OnBuffersSwapped(const gfx::Size& size,
                                               const std::string& mailbox_name,
                                               int gpu_route_id,
                                               int gpu_host_id) {
  if (!last_mailbox_valid_)
    SendACK(std::string(), host_routing_id_, gpu_route_id, gpu_host_id, 0);

  last_mailbox_valid_ = !mailbox_name.empty();
  texture_layer_->setTextureMailbox(mailbox_name,
                                    base::Bind(&SendACK,
                                               mailbox_name,
                                               host_routing_id_,
                                               gpu_route_id,
                                               gpu_host_id));
}

}  // namespace content
