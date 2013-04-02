// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_compositing_helper.h"

#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/context_provider.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size_conversions.h"
#include "webkit/compositor_bindings/web_layer_impl.h"

namespace content {

BrowserPluginCompositingHelper::BrowserPluginCompositingHelper(
    WebKit::WebPluginContainer* container,
    BrowserPluginManager* manager,
    int instance_id,
    int host_routing_id)
    : instance_id_(instance_id),
      host_routing_id_(host_routing_id),
      last_route_id_(0),
      last_host_id_(0),
      last_mailbox_valid_(false),
      ack_pending_(true),
      ack_pending_for_crashed_guest_(false),
      container_(container),
      browser_plugin_manager_(manager) {
}

BrowserPluginCompositingHelper::~BrowserPluginCompositingHelper() {
}

void BrowserPluginCompositingHelper::DidCommitCompositorFrame() {
  if (!delegated_layer_ || !ack_pending_)
    return;

  cc::CompositorFrameAck ack;
  delegated_layer_->TakeUnusedResourcesForChildCompositor(&ack.resources);

  browser_plugin_manager_->Send(
      new BrowserPluginHostMsg_CompositorFrameACK(
          host_routing_id_,
          instance_id_,
          last_route_id_,
          last_host_id_,
          ack));

  ack_pending_ = false;
}

void BrowserPluginCompositingHelper::EnableCompositing(bool enable) {
  if (enable && !background_layer_) {
    background_layer_ = cc::SolidColorLayer::Create();
    background_layer_->SetMasksToBounds(true);
    background_layer_->SetBackgroundColor(
        SkColorSetARGBInline(255, 255, 255, 255));
    web_layer_.reset(new webkit::WebLayerImpl(background_layer_));
  }

  container_->setWebLayer(enable ? web_layer_.get() : NULL);
}

void BrowserPluginCompositingHelper::CheckSizeAndAdjustLayerBounds(
    const gfx::Size& new_size,
    float device_scale_factor,
    cc::Layer* layer) {
  if (buffer_size_ != new_size) {
    buffer_size_ = new_size;
    // The container size is in DIP, so is the layer size.
    // Buffer size is in physical pixels, so we need to adjust
    // it by the device scale factor.
    gfx::Size device_scale_adjusted_size = gfx::ToFlooredSize(
        gfx::ScaleSize(buffer_size_, 1.0f / device_scale_factor));
    layer->SetBounds(device_scale_adjusted_size);
  }
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

  scoped_refptr<cc::ContextProvider> context_provider =
      RenderThreadImpl::current()->OffscreenContextProviderForMainThread();
  if (!context_provider)
    return;

  WebKit::WebGraphicsContext3D *context = context_provider->Context3d();
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
  // This means the GPU process crashed and we have nothing further to do.
  // Nobody is expecting an ACK and the buffer doesn't need to be deleted
  // because it went away with the GPU process.
  if (last_host_id_ != gpu_host_id)
    return;

  // This means the guest crashed.
  // Either ACK the last buffer, so texture transport could
  // be destroyed of delete the mailbox if nobody wants it back.
  if (last_route_id_ != gpu_route_id) {
    if (!ack_pending_for_crashed_guest_) {
      FreeMailboxMemory(mailbox_name, sync_point);
    } else {
      ack_pending_for_crashed_guest_ = false;
      browser_plugin_manager_->Send(
          new BrowserPluginHostMsg_BuffersSwappedACK(
              host_routing_id_,
              instance_id_,
              gpu_route_id,
              gpu_host_id,
              mailbox_name,
              sync_point));
    }
    return;
  }

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
          instance_id_,
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
  delegated_layer_ = NULL;
  background_layer_ = NULL;
  web_layer_.reset();
}

void BrowserPluginCompositingHelper::OnBuffersSwapped(
    const gfx::Size& size,
    const std::string& mailbox_name,
    int gpu_route_id,
    int gpu_host_id,
    float device_scale_factor) {
  DCHECK(!delegated_layer_);
  // If the guest crashed but the GPU process didn't, we may still have
  // a transport surface waiting on an ACK, which we must send to
  // avoid leaking.
  if (last_route_id_ != gpu_route_id && last_host_id_ == gpu_host_id)
    ack_pending_for_crashed_guest_ = ack_pending_;

  // If these mismatch, we are either just starting up, GPU process crashed or
  // guest renderer crashed.
  // In this case, we are communicating with a new image transport
  // surface and must ACK with the new ID's and an empty mailbox.
  if (last_route_id_ != gpu_route_id || last_host_id_ != gpu_host_id)
    last_mailbox_valid_ = false;

  last_route_id_ = gpu_route_id;
  last_host_id_ = gpu_host_id;

  ack_pending_ = true;
  // Browser plugin getting destroyed, do a fast ACK.
  if (!background_layer_) {
    MailboxReleased(mailbox_name, gpu_route_id, gpu_host_id, 0);
    return;
  }

  if (!texture_layer_) {
    texture_layer_ = cc::TextureLayer::CreateForMailbox();
    texture_layer_->SetIsDrawable(true);
    texture_layer_->SetContentsOpaque(true);

    background_layer_->AddChild(texture_layer_);
  }

  // The size of browser plugin container is not always equal to the size
  // of the buffer that arrives here. This could be for a number of reasons,
  // including autosize and a resize in progress.
  // During resize, the container size changes first and then some time
  // later, a new buffer with updated size will arrive. During this process,
  // we need to make sure that things are still displayed pixel perfect.
  // We accomplish this by modifying bounds of the texture layer only
  // when a new buffer arrives.
  // Visually, this will either display a smaller part of the buffer
  // or introduce a gutter around it.
  CheckSizeAndAdjustLayerBounds(size,
                                device_scale_factor,
                                texture_layer_.get());

  bool current_mailbox_valid = !mailbox_name.empty();
  if (!last_mailbox_valid_) {
    MailboxReleased(std::string(), gpu_route_id, gpu_host_id, 0);
    if (!current_mailbox_valid)
      return;
  }

  cc::TextureMailbox::ReleaseCallback callback;
  if (current_mailbox_valid) {
    callback = base::Bind(&BrowserPluginCompositingHelper::MailboxReleased,
                          scoped_refptr<BrowserPluginCompositingHelper>(this),
                          mailbox_name,
                          gpu_route_id,
                          gpu_host_id);
  }
  texture_layer_->SetTextureMailbox(cc::TextureMailbox(mailbox_name,
                                                       callback));
  texture_layer_->SetNeedsDisplay();
  last_mailbox_valid_ = current_mailbox_valid;
}

void BrowserPluginCompositingHelper::OnCompositorFrameSwapped(
    scoped_ptr<cc::CompositorFrame> frame,
    int route_id,
    int host_id) {
  DCHECK(!texture_layer_);
  if (!delegated_layer_) {
    delegated_layer_ = cc::DelegatedRendererLayer::Create();
    delegated_layer_->SetIsDrawable(true);
    delegated_layer_->SetContentsOpaque(true);

    background_layer_->AddChild(delegated_layer_);
  }

  cc::DelegatedFrameData *frame_data = frame->delegated_frame_data.get();
  if (!frame_data)
    return;

  CheckSizeAndAdjustLayerBounds(
      frame_data->render_pass_list.back()->output_rect.size(),
      frame->metadata.device_scale_factor,
      delegated_layer_.get());

  delegated_layer_->SetFrameData(frame->delegated_frame_data.Pass());
  last_route_id_ = route_id;
  last_host_id_ = host_id;
  ack_pending_ = true;
}

void BrowserPluginCompositingHelper::UpdateVisibility(bool visible) {
  if (texture_layer_)
    texture_layer_->SetIsDrawable(visible);
  if (delegated_layer_)
    delegated_layer_->SetIsDrawable(visible);
}

}  // namespace content
