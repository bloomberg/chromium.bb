// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_compositing_helper.h"

#include "cc/texture_layer.h"
#include "content/common/browser_plugin_messages.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSharedGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

namespace content {

BrowserPluginCompositingHelper::BrowserPluginCompositingHelper(
    WebKit::WebPluginContainer* container,
    BrowserPluginManager* manager,
    int host_routing_id)
    : host_routing_id_(host_routing_id),
      last_mailbox_valid_(false),
      ack_pending_(true),
      container_(container),
      browser_plugin_manager_(manager) {
}

BrowserPluginCompositingHelper::~BrowserPluginCompositingHelper() {
}

void BrowserPluginCompositingHelper::EnableCompositing(bool enable) {
  if (enable && !texture_layer_) {
    texture_layer_ = cc::TextureLayer::createForMailbox();
    web_layer_.reset(new WebKit::WebLayerImpl(texture_layer_));
  }

  container_->setWebLayer(enable ? web_layer_.get() : NULL);
}

// If we have a mailbox that was freed up from the compositor,
// but we are not expected to return it to the guest renderer
// via an ACK, we should free it because we now own it.
// To free the mailbox memory, we need a context to consume it
// into a texture ID and then delete this texture ID.
// We use a shared graphics context accessible from the main
// thread to do it.
void BrowserPluginCompositingHelper::FreeMailboxMemory(
    const std::string& mailbox_name,
    unsigned sync_point) {
  if (mailbox_name.empty())
    return;

  WebKit::WebGraphicsContext3D *context =
     WebKit::WebSharedGraphicsContext3D::mainThreadContext();
  DCHECK(context);
  // When a buffer is released from the compositor, we also get a
  // sync point that specifies when in the command buffer
  // it's safe to use it again.
  // If the sync point is non-zero, we need to tell our context
  // to wait until this sync point is reached before we can safely
  // delete the buffer.
  if (sync_point)
    context->waitSyncPoint(sync_point);

  unsigned texture_id = context->createTexture();
  context->bindTexture(GL_TEXTURE_2D, texture_id);
  context->consumeTextureCHROMIUM(
      GL_TEXTURE_2D,
      reinterpret_cast<const int8*>(mailbox_name.data()));
  context->deleteTexture(texture_id);
}

void BrowserPluginCompositingHelper::MailboxReleased(
    const std::string& mailbox_name,
    int gpu_route_id,
    int gpu_host_id,
    unsigned sync_point) {
  // We need to send an ACK to TextureImageTransportSurface
  // for every buffer it sends us. However, if a buffer is freed up from
  // the compositor in cases like switching back to SW mode without a new
  // buffer arriving, no ACK is needed and we destroy this buffer.
  if (!ack_pending_) {
    FreeMailboxMemory(mailbox_name, sync_point);
    last_mailbox_valid_ = false;
    return;
  }
  ack_pending_ = false;
  browser_plugin_manager_->Send(
      new BrowserPluginHostMsg_BuffersSwappedACK(
          host_routing_id_,
          gpu_route_id,
          gpu_host_id,
          mailbox_name,
          sync_point));
}

void BrowserPluginCompositingHelper::OnContainerDestroy() {
  if (container_)
    container_->setWebLayer(NULL);
  container_ = NULL;

  texture_layer_ = NULL;
  web_layer_.reset();
}

void BrowserPluginCompositingHelper::OnBuffersSwapped(
    const gfx::Size& size,
    const std::string& mailbox_name,
    int gpu_route_id,
    int gpu_host_id) {
  ack_pending_ = true;
  // Browser plugin getting destroyed, do a fast ACK.
  if (!texture_layer_) {
    MailboxReleased(mailbox_name, gpu_route_id, gpu_host_id, 0);
    return;
  }

  // The size of browser plugin container is not always equal to the size
  // of the buffer that arrives here. This could be for a number of reasons,
  // including autosize and a resize in progress.
  // During resize, the container size changes first and then some time
  // later, a new buffer with updated size will arrive. During this process,
  // we need to make sure that things are still displayed pixel perfect.
  // We accomplish this by modifying texture coordinates in the layer,
  // and either buffer size or container size change triggers the need
  // to also update texture coordinates. Visually, this will either
  // display a smaller part of the buffer or introduce a gutter around it.
  if (buffer_size_ != size) {
    buffer_size_ = size;
    UpdateUVRect();
  }

  bool current_mailbox_valid = !mailbox_name.empty();
  if (!last_mailbox_valid_) {
    MailboxReleased(std::string(), gpu_route_id, gpu_host_id, 0);
    if (!current_mailbox_valid)
      return;
  }

  cc::TextureLayer::MailboxCallback callback;
  if (current_mailbox_valid) {
    callback = base::Bind(&BrowserPluginCompositingHelper::MailboxReleased,
                          scoped_refptr<BrowserPluginCompositingHelper>(this),
                          mailbox_name,
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
