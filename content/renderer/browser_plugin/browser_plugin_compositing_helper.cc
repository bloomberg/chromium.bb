// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin_compositing_helper.h"

#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/output/context_provider.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/platform/WebGraphicsContext3D.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"
#include "webkit/renderer/compositor_bindings/web_layer_impl.h"

namespace content {

BrowserPluginCompositingHelper::SwapBuffersInfo::SwapBuffersInfo()
    : route_id(0),
      output_surface_id(0),
      host_id(0),
      software_frame_id(0),
      shared_memory(NULL) {
}

BrowserPluginCompositingHelper::BrowserPluginCompositingHelper(
    blink::WebPluginContainer* container,
    BrowserPluginManager* manager,
    int instance_id,
    int host_routing_id)
    : instance_id_(instance_id),
      host_routing_id_(host_routing_id),
      last_route_id_(0),
      last_output_surface_id_(0),
      last_host_id_(0),
      last_mailbox_valid_(false),
      ack_pending_(true),
      software_ack_pending_(false),
      opaque_(true),
      container_(container),
      browser_plugin_manager_(manager) {
}

BrowserPluginCompositingHelper::~BrowserPluginCompositingHelper() {
}

void BrowserPluginCompositingHelper::CopyFromCompositingSurface(
    int request_id,
    gfx::Rect source_rect,
    gfx::Size dest_size) {
  CHECK(background_layer_);
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(base::Bind(
          &BrowserPluginCompositingHelper::CopyFromCompositingSurfaceHasResult,
          this,
          request_id,
          dest_size));
  request->set_area(source_rect);
  background_layer_->RequestCopyOfOutput(request.Pass());
}

void BrowserPluginCompositingHelper::DidCommitCompositorFrame() {
  if (software_ack_pending_) {
    cc::CompositorFrameAck ack;
    if (!unacked_software_frames_.empty()) {
      ack.last_software_frame_id = unacked_software_frames_.back();
      unacked_software_frames_.pop_back();
    }

    browser_plugin_manager_->Send(
        new BrowserPluginHostMsg_CompositorFrameACK(
            host_routing_id_,
            instance_id_,
            last_route_id_,
            last_output_surface_id_,
            last_host_id_,
            ack));

    software_ack_pending_ = false;
  }
  if (!resource_collection_.get() || !ack_pending_)
    return;

  cc::CompositorFrameAck ack;
  resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);

  browser_plugin_manager_->Send(
      new BrowserPluginHostMsg_CompositorFrameACK(
          host_routing_id_,
          instance_id_,
          last_route_id_,
          last_output_surface_id_,
          last_host_id_,
          ack));

  ack_pending_ = false;
}

void BrowserPluginCompositingHelper::EnableCompositing(bool enable) {
  if (enable && !background_layer_.get()) {
    background_layer_ = cc::SolidColorLayer::Create();
    background_layer_->SetMasksToBounds(true);
    background_layer_->SetBackgroundColor(
        SkColorSetARGBInline(255, 255, 255, 255));
    web_layer_.reset(new webkit::WebLayerImpl(background_layer_));
  }

  container_->setWebLayer(enable ? web_layer_.get() : NULL);
}

void BrowserPluginCompositingHelper::CheckSizeAndAdjustLayerProperties(
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

  // Manually manage background layer for transparent webview.
  if (!opaque_)
    background_layer_->SetIsDrawable(false);
}

void BrowserPluginCompositingHelper::MailboxReleased(
    SwapBuffersInfo mailbox,
    unsigned sync_point,
    bool lost_resource) {
  if (mailbox.type == SOFTWARE_COMPOSITOR_FRAME) {
    delete mailbox.shared_memory;
    mailbox.shared_memory = NULL;
  } else if (lost_resource) {
    // Reset mailbox's name if the resource was lost.
    mailbox.name.SetZero();
  }

  // This means the GPU process crashed or guest crashed.
  if (last_host_id_ != mailbox.host_id ||
      last_output_surface_id_ != mailbox.output_surface_id ||
      last_route_id_ != mailbox.route_id)
    return;

  if (mailbox.type == SOFTWARE_COMPOSITOR_FRAME)
    unacked_software_frames_.push_back(mailbox.software_frame_id);

  // We need to send an ACK to for every buffer sent to us.
  // However, if a buffer is freed up from
  // the compositor in cases like switching back to SW mode without a new
  // buffer arriving, no ACK is needed.
  if (!ack_pending_) {
    last_mailbox_valid_ = false;
    return;
  }
  ack_pending_ = false;
  switch (mailbox.type) {
    case TEXTURE_IMAGE_TRANSPORT: {
      std::string mailbox_name(reinterpret_cast<const char*>(mailbox.name.name),
                               sizeof(mailbox.name.name));
      browser_plugin_manager_->Send(
          new BrowserPluginHostMsg_BuffersSwappedACK(
              host_routing_id_,
              instance_id_,
              mailbox.route_id,
              mailbox.host_id,
              mailbox_name,
              sync_point));
      break;
    }
    case GL_COMPOSITOR_FRAME: {
      cc::CompositorFrameAck ack;
      ack.gl_frame_data.reset(new cc::GLFrameData());
      ack.gl_frame_data->mailbox = mailbox.name;
      ack.gl_frame_data->size = mailbox.size;
      ack.gl_frame_data->sync_point = sync_point;

      browser_plugin_manager_->Send(
         new BrowserPluginHostMsg_CompositorFrameACK(
             host_routing_id_,
             instance_id_,
             mailbox.route_id,
             mailbox.output_surface_id,
             mailbox.host_id,
             ack));
      break;
    }
    case SOFTWARE_COMPOSITOR_FRAME:
      break;
  }
}

void BrowserPluginCompositingHelper::OnContainerDestroy() {
  if (container_)
    container_->setWebLayer(NULL);
  container_ = NULL;

  if (resource_collection_)
    resource_collection_->SetClient(NULL);

  ack_pending_ = false;
  software_ack_pending_ = false;
  resource_collection_ = NULL;
  frame_provider_ = NULL;
  texture_layer_ = NULL;
  delegated_layer_ = NULL;
  background_layer_ = NULL;
  web_layer_.reset();
}

void BrowserPluginCompositingHelper::OnBuffersSwappedPrivate(
    const SwapBuffersInfo& mailbox,
    unsigned sync_point,
    float device_scale_factor) {
  DCHECK(!delegated_layer_.get());
  // If these mismatch, we are either just starting up, GPU process crashed or
  // guest renderer crashed.
  // In this case, we are communicating with a new image transport
  // surface and must ACK with the new ID's and an empty mailbox.
  if (last_route_id_ != mailbox.route_id ||
      last_output_surface_id_ != mailbox.output_surface_id ||
      last_host_id_ != mailbox.host_id)
    last_mailbox_valid_ = false;

  last_route_id_ = mailbox.route_id;
  last_output_surface_id_ = mailbox.output_surface_id;
  last_host_id_ = mailbox.host_id;

  ack_pending_ = true;
  // Browser plugin getting destroyed, do a fast ACK.
  if (!background_layer_.get()) {
    MailboxReleased(mailbox, sync_point, false);
    return;
  }

  if (!texture_layer_.get()) {
    texture_layer_ = cc::TextureLayer::CreateForMailbox(NULL);
    texture_layer_->SetIsDrawable(true);
    SetContentsOpaque(opaque_);

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
  CheckSizeAndAdjustLayerProperties(mailbox.size,
                                    device_scale_factor,
                                    texture_layer_.get());

  bool is_software_frame = mailbox.type == SOFTWARE_COMPOSITOR_FRAME;
  bool current_mailbox_valid = is_software_frame ?
      mailbox.shared_memory != NULL : !mailbox.name.IsZero();
  if (!is_software_frame && !last_mailbox_valid_) {
    SwapBuffersInfo empty_info = mailbox;
    empty_info.name.SetZero();
    MailboxReleased(empty_info, 0, false);
    if (!current_mailbox_valid)
      return;
  }

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> release_callback;
  if (current_mailbox_valid) {
    release_callback = cc::SingleReleaseCallback::Create(
        base::Bind(&BrowserPluginCompositingHelper::MailboxReleased,
                   scoped_refptr<BrowserPluginCompositingHelper>(this),
                   mailbox)).Pass();
    if (is_software_frame)
      texture_mailbox = cc::TextureMailbox(mailbox.shared_memory, mailbox.size);
    else
      texture_mailbox = cc::TextureMailbox(mailbox.name, sync_point);
  }

  texture_layer_->SetFlipped(!is_software_frame);
  texture_layer_->SetTextureMailbox(texture_mailbox, release_callback.Pass());
  texture_layer_->SetNeedsDisplay();
  last_mailbox_valid_ = current_mailbox_valid;
}

void BrowserPluginCompositingHelper::OnBuffersSwapped(
    const gfx::Size& size,
    const std::string& mailbox_name,
    int gpu_route_id,
    int gpu_host_id,
    float device_scale_factor) {
  SwapBuffersInfo swap_info;
  swap_info.name.SetName(reinterpret_cast<const int8*>(mailbox_name.data()));
  swap_info.type = TEXTURE_IMAGE_TRANSPORT;
  swap_info.size = size;
  swap_info.route_id = gpu_route_id;
  swap_info.output_surface_id = 0;
  swap_info.host_id = gpu_host_id;
  OnBuffersSwappedPrivate(swap_info, 0, device_scale_factor);
}

void BrowserPluginCompositingHelper::OnCompositorFrameSwapped(
    scoped_ptr<cc::CompositorFrame> frame,
    int route_id,
    uint32 output_surface_id,
    int host_id) {
  if (frame->gl_frame_data) {
    SwapBuffersInfo swap_info;
    swap_info.name = frame->gl_frame_data->mailbox;
    swap_info.type = GL_COMPOSITOR_FRAME;
    swap_info.size = frame->gl_frame_data->size;
    swap_info.route_id = route_id;
    swap_info.output_surface_id = output_surface_id;
    swap_info.host_id = host_id;
    OnBuffersSwappedPrivate(swap_info,
                            frame->gl_frame_data->sync_point,
                            frame->metadata.device_scale_factor);
    return;
  }

  if (frame->software_frame_data) {
    cc::SoftwareFrameData* frame_data = frame->software_frame_data.get();

    SwapBuffersInfo swap_info;
    swap_info.type = SOFTWARE_COMPOSITOR_FRAME;
    swap_info.size = frame_data->size;
    swap_info.route_id = route_id;
    swap_info.output_surface_id = output_surface_id;
    swap_info.host_id = host_id;
    swap_info.software_frame_id = frame_data->id;

    scoped_ptr<base::SharedMemory> shared_memory(
        new base::SharedMemory(frame_data->handle, true));
    const size_t size_in_bytes = 4 * frame_data->size.GetArea();
    if (!shared_memory->Map(size_in_bytes)) {
      LOG(ERROR) << "Failed to map shared memory of size "
                 << size_in_bytes;
      // Send ACK right away.
      software_ack_pending_ = true;
      MailboxReleased(swap_info, 0, false);
      DidCommitCompositorFrame();
      return;
    }

    swap_info.shared_memory = shared_memory.release();
    OnBuffersSwappedPrivate(swap_info, 0,
                            frame->metadata.device_scale_factor);
    software_ack_pending_ = true;
    last_route_id_ = route_id;
    last_output_surface_id_ = output_surface_id;
    last_host_id_ = host_id;
    return;
  }

  DCHECK(!texture_layer_.get());

  cc::DelegatedFrameData* frame_data = frame->delegated_frame_data.get();
  // Do nothing if we are getting destroyed or have no frame data.
  if (!frame_data || !background_layer_)
    return;

  DCHECK(!frame_data->render_pass_list.empty());
  cc::RenderPass* root_pass = frame_data->render_pass_list.back();
  gfx::Size frame_size = root_pass->output_rect.size();

  if (last_route_id_ != route_id ||
      last_output_surface_id_ != output_surface_id ||
      last_host_id_ != host_id) {
    // Resource ids are scoped by the output surface.
    // If the originating output surface doesn't match the last one, it
    // indicates the guest's output surface may have been recreated, in which
    // case we should recreate the DelegatedRendererLayer, to avoid matching
    // resources from the old one with resources from the new one which would
    // have the same id.
    frame_provider_ = NULL;

    // Drop the cc::DelegatedFrameResourceCollection so that we will not return
    // any resources from the old output surface with the new output surface id.
    if (resource_collection_) {
      resource_collection_->SetClient(NULL);

      if (resource_collection_->LoseAllResources())
        SendReturnedDelegatedResources();
      resource_collection_ = NULL;
    }
    last_output_surface_id_ = output_surface_id;
    last_route_id_ = route_id;
    last_host_id_ = host_id;
  }
  if (!resource_collection_) {
    resource_collection_ = new cc::DelegatedFrameResourceCollection;
    resource_collection_->SetClient(this);
  }
  if (!frame_provider_.get() || frame_provider_->frame_size() != frame_size) {
    frame_provider_ = new cc::DelegatedFrameProvider(
        resource_collection_.get(), frame->delegated_frame_data.Pass());
    if (delegated_layer_.get())
      delegated_layer_->RemoveFromParent();
    delegated_layer_ =
        cc::DelegatedRendererLayer::Create(frame_provider_.get());
    delegated_layer_->SetIsDrawable(true);
    SetContentsOpaque(opaque_);
    background_layer_->AddChild(delegated_layer_);
  } else {
    frame_provider_->SetFrameData(frame->delegated_frame_data.Pass());
  }

  CheckSizeAndAdjustLayerProperties(
      frame_data->render_pass_list.back()->output_rect.size(),
      frame->metadata.device_scale_factor,
      delegated_layer_.get());

  ack_pending_ = true;
}

void BrowserPluginCompositingHelper::UpdateVisibility(bool visible) {
  if (texture_layer_.get())
    texture_layer_->SetIsDrawable(visible);
  if (delegated_layer_.get())
    delegated_layer_->SetIsDrawable(visible);
}

void BrowserPluginCompositingHelper::UnusedResourcesAreAvailable() {
  if (ack_pending_)
    return;

  SendReturnedDelegatedResources();
}

void BrowserPluginCompositingHelper::SendReturnedDelegatedResources() {
  cc::CompositorFrameAck ack;
  if (resource_collection_)
    resource_collection_->TakeUnusedResourcesForChildCompositor(&ack.resources);
  DCHECK(!ack.resources.empty());

  browser_plugin_manager_->Send(
      new BrowserPluginHostMsg_ReclaimCompositorResources(
          host_routing_id_,
          instance_id_,
          last_route_id_,
          last_output_surface_id_,
          last_host_id_,
          ack));
}

void BrowserPluginCompositingHelper::SetContentsOpaque(bool opaque) {
  opaque_ = opaque;

  if (texture_layer_.get())
    texture_layer_->SetContentsOpaque(opaque_);
  if (delegated_layer_.get())
    delegated_layer_->SetContentsOpaque(opaque_);
}

void BrowserPluginCompositingHelper::CopyFromCompositingSurfaceHasResult(
    int request_id,
    gfx::Size dest_size,
    scoped_ptr<cc::CopyOutputResult> result) {
  scoped_ptr<SkBitmap> bitmap;
  if (result && result->HasBitmap() && !result->size().IsEmpty())
    bitmap = result->TakeBitmap();

  SkBitmap resized_bitmap;
  if (bitmap) {
    resized_bitmap = skia::ImageOperations::Resize(*bitmap,
                       skia::ImageOperations::RESIZE_BEST,
                       dest_size.width(),
                       dest_size.height());
  }
  browser_plugin_manager_->Send(
      new BrowserPluginHostMsg_CopyFromCompositingSurfaceAck(
          host_routing_id_, instance_id_, request_id,
          resized_bitmap));
}

}  // namespace content
