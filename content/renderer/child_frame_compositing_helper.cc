// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/child_frame_compositing_helper.h"

#include "cc/blink/web_layer_impl.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/output/context_provider.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/gpu/client/context_provider_command_buffer.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"

namespace content {

ChildFrameCompositingHelper*
ChildFrameCompositingHelper::CreateForBrowserPlugin(
    const base::WeakPtr<BrowserPlugin>& browser_plugin) {
  return new ChildFrameCompositingHelper(
      browser_plugin, NULL, NULL, browser_plugin->render_view_routing_id());
}

ChildFrameCompositingHelper*
ChildFrameCompositingHelper::CreateForRenderFrameProxy(
    RenderFrameProxy* render_frame_proxy) {
  return new ChildFrameCompositingHelper(base::WeakPtr<BrowserPlugin>(),
                                         render_frame_proxy->web_frame(),
                                         render_frame_proxy,
                                         render_frame_proxy->routing_id());
}

ChildFrameCompositingHelper::ChildFrameCompositingHelper(
    const base::WeakPtr<BrowserPlugin>& browser_plugin,
    blink::WebFrame* frame,
    RenderFrameProxy* render_frame_proxy,
    int host_routing_id)
    : host_routing_id_(host_routing_id),
      last_route_id_(0),
      last_output_surface_id_(0),
      last_host_id_(0),
      ack_pending_(true),
      opaque_(true),
      browser_plugin_(browser_plugin),
      render_frame_proxy_(render_frame_proxy),
      frame_(frame) {}

ChildFrameCompositingHelper::~ChildFrameCompositingHelper() {}

BrowserPluginManager* ChildFrameCompositingHelper::GetBrowserPluginManager() {
  if (!browser_plugin_)
    return NULL;

  return browser_plugin_->browser_plugin_manager();
}

blink::WebPluginContainer* ChildFrameCompositingHelper::GetContainer() {
  if (!browser_plugin_)
    return NULL;

  return browser_plugin_->container();
}

int ChildFrameCompositingHelper::GetInstanceID() {
  if (!browser_plugin_)
    return 0;

  return browser_plugin_->browser_plugin_instance_id();
}

void ChildFrameCompositingHelper::SendCompositorFrameSwappedACKToBrowser(
    FrameHostMsg_CompositorFrameSwappedACK_Params& params) {
  // This function will be removed when BrowserPluginManager is removed and
  // BrowserPlugin is modified to use a RenderFrame.
  if (GetBrowserPluginManager()) {
    GetBrowserPluginManager()->Send(
        new BrowserPluginHostMsg_CompositorFrameSwappedACK(
            host_routing_id_, GetInstanceID(), params));
  } else if (render_frame_proxy_) {
    render_frame_proxy_->Send(
        new FrameHostMsg_CompositorFrameSwappedACK(host_routing_id_, params));
  }
}

void ChildFrameCompositingHelper::SendReclaimCompositorResourcesToBrowser(
    FrameHostMsg_ReclaimCompositorResources_Params& params) {
  // This function will be removed when BrowserPluginManager is removed and
  // BrowserPlugin is modified to use a RenderFrame.
  if (GetBrowserPluginManager()) {
    GetBrowserPluginManager()->Send(
        new BrowserPluginHostMsg_ReclaimCompositorResources(
            host_routing_id_, GetInstanceID(), params));
  } else if (render_frame_proxy_) {
    render_frame_proxy_->Send(
        new FrameHostMsg_ReclaimCompositorResources(host_routing_id_, params));
  }
}

void ChildFrameCompositingHelper::CopyFromCompositingSurface(
    int request_id,
    gfx::Rect source_rect,
    gfx::Size dest_size) {
  CHECK(background_layer_.get());
  scoped_ptr<cc::CopyOutputRequest> request =
      cc::CopyOutputRequest::CreateBitmapRequest(base::Bind(
          &ChildFrameCompositingHelper::CopyFromCompositingSurfaceHasResult,
          this,
          request_id,
          dest_size));
  request->set_area(source_rect);
  background_layer_->RequestCopyOfOutput(request.Pass());
}

void ChildFrameCompositingHelper::DidCommitCompositorFrame() {
  if (!resource_collection_.get() || !ack_pending_)
    return;

  FrameHostMsg_CompositorFrameSwappedACK_Params params;
  params.producing_host_id = last_host_id_;
  params.producing_route_id = last_route_id_;
  params.output_surface_id = last_output_surface_id_;
  resource_collection_->TakeUnusedResourcesForChildCompositor(
      &params.ack.resources);

  SendCompositorFrameSwappedACKToBrowser(params);

  ack_pending_ = false;
}

void ChildFrameCompositingHelper::EnableCompositing(bool enable) {
  if (enable && !background_layer_.get()) {
    background_layer_ = cc::SolidColorLayer::Create();
    background_layer_->SetMasksToBounds(true);
    background_layer_->SetBackgroundColor(
        SkColorSetARGBInline(255, 255, 255, 255));
    web_layer_.reset(new cc_blink::WebLayerImpl(background_layer_));
  }

  if (GetContainer()) {
    GetContainer()->setWebLayer(enable ? web_layer_.get() : NULL);
  } else if (frame_) {
    frame_->setRemoteWebLayer(enable ? web_layer_.get() : NULL);
  }
}

void ChildFrameCompositingHelper::CheckSizeAndAdjustLayerProperties(
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

void ChildFrameCompositingHelper::OnContainerDestroy() {
  if (GetContainer())
    GetContainer()->setWebLayer(NULL);

  if (resource_collection_.get())
    resource_collection_->SetClient(NULL);

  ack_pending_ = false;
  resource_collection_ = NULL;
  frame_provider_ = NULL;
  delegated_layer_ = NULL;
  background_layer_ = NULL;
  web_layer_.reset();
}

void ChildFrameCompositingHelper::ChildFrameGone() {
  background_layer_->SetBackgroundColor(SkColorSetARGBInline(255, 0, 128, 0));
  background_layer_->RemoveAllChildren();
  background_layer_->SetIsDrawable(true);
  background_layer_->SetContentsOpaque(true);
}

void ChildFrameCompositingHelper::OnCompositorFrameSwapped(
    scoped_ptr<cc::CompositorFrame> frame,
    int route_id,
    uint32 output_surface_id,
    int host_id,
    base::SharedMemoryHandle handle) {
  cc::DelegatedFrameData* frame_data = frame->delegated_frame_data.get();
  // Do nothing if we are getting destroyed or have no frame data.
  if (!frame_data || !background_layer_.get())
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
    if (resource_collection_.get()) {
      resource_collection_->SetClient(NULL);

      if (resource_collection_->LoseAllResources())
        SendReturnedDelegatedResources();
      resource_collection_ = NULL;
    }
    last_output_surface_id_ = output_surface_id;
    last_route_id_ = route_id;
    last_host_id_ = host_id;
  }
  if (!resource_collection_.get()) {
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
    buffer_size_ = gfx::Size();
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

void ChildFrameCompositingHelper::UpdateVisibility(bool visible) {
  if (delegated_layer_.get())
    delegated_layer_->SetIsDrawable(visible);
}

void ChildFrameCompositingHelper::UnusedResourcesAreAvailable() {
  if (ack_pending_)
    return;

  SendReturnedDelegatedResources();
}

void ChildFrameCompositingHelper::SendReturnedDelegatedResources() {
  FrameHostMsg_ReclaimCompositorResources_Params params;
  if (resource_collection_.get())
    resource_collection_->TakeUnusedResourcesForChildCompositor(
        &params.ack.resources);
  DCHECK(!params.ack.resources.empty());

  params.route_id = last_route_id_;
  params.output_surface_id = last_output_surface_id_;
  params.renderer_host_id = last_host_id_;
  SendReclaimCompositorResourcesToBrowser(params);
}

void ChildFrameCompositingHelper::SetContentsOpaque(bool opaque) {
  opaque_ = opaque;
  if (delegated_layer_.get())
    delegated_layer_->SetContentsOpaque(opaque_);
}

void ChildFrameCompositingHelper::CopyFromCompositingSurfaceHasResult(
    int request_id,
    gfx::Size dest_size,
    scoped_ptr<cc::CopyOutputResult> result) {
  scoped_ptr<SkBitmap> bitmap;
  if (result && result->HasBitmap() && !result->size().IsEmpty())
    bitmap = result->TakeBitmap();

  SkBitmap resized_bitmap;
  if (bitmap) {
    resized_bitmap =
        skia::ImageOperations::Resize(*bitmap,
                                      skia::ImageOperations::RESIZE_BEST,
                                      dest_size.width(),
                                      dest_size.height());
  }
  if (GetBrowserPluginManager()) {
    GetBrowserPluginManager()->Send(
        new BrowserPluginHostMsg_CopyFromCompositingSurfaceAck(
            host_routing_id_, GetInstanceID(), request_id, resized_bitmap));
  }
}

}  // namespace content
