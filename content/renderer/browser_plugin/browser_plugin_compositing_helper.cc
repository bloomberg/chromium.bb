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

void BrowserPluginCompositingHelper::OnBuffersSwapped(
    const gfx::Size& size,
    const std::string& mailbox_name,
    int gpu_route_id,
    int gpu_host_id) {
  if (buffer_size_ != size) {
    buffer_size_ = size;
    UpdateUVRect();
  }
  if (!last_mailbox_valid_)
    SendACK(std::string(), host_routing_id_, gpu_route_id, gpu_host_id, 0);

  bool current_mailbox_valid = !mailbox_name.empty();
  if (!current_mailbox_valid && !last_mailbox_valid_)
    return;

  cc::TextureLayer::MailboxCallback callback;
  if (current_mailbox_valid) {
    callback = base::Bind(&SendACK,
                          mailbox_name,
                          host_routing_id_,
                          gpu_route_id,
                          gpu_host_id);
  }
  texture_layer_->setTextureMailbox(mailbox_name, callback);
  last_mailbox_valid_ = current_mailbox_valid;
}

void BrowserPluginCompositingHelper::SetContainerSize(const gfx::Size& size) {
  if (container_size_ == size)
    return;

  container_size_ = size;
  UpdateUVRect();
}

void BrowserPluginCompositingHelper::UpdateUVRect() {
  if (!texture_layer_)
    return;

  gfx::RectF uv_rect(0, 0, 1, 1);
  if (!buffer_size_.IsEmpty() && !container_size_.IsEmpty()) {
    uv_rect.set_width(static_cast<float>(container_size_.width()) /
                      static_cast<float>(buffer_size_.width()));
    uv_rect.set_height(static_cast<float>(container_size_.height()) /
                       static_cast<float>(buffer_size_.height()));
  }
  texture_layer_->setUV(uv_rect.origin(), uv_rect.bottom_right());
}

}  // namespace content
