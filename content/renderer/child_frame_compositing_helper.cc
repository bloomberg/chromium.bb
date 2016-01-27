// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/child_frame_compositing_helper.h"

#include <utility>

#include "cc/blink/web_layer_impl.h"
#include "cc/layers/delegated_frame_provider.h"
#include "cc/layers/delegated_frame_resource_collection.h"
#include "cc/layers/delegated_renderer_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/context_provider.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_switches_internal.h"
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
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"

namespace content {

ChildFrameCompositingHelper*
ChildFrameCompositingHelper::CreateForBrowserPlugin(
    const base::WeakPtr<BrowserPlugin>& browser_plugin) {
  return new ChildFrameCompositingHelper(
      browser_plugin, nullptr, nullptr,
      browser_plugin->render_frame_routing_id());
}

ChildFrameCompositingHelper*
ChildFrameCompositingHelper::CreateForRenderFrameProxy(
    RenderFrameProxy* render_frame_proxy) {
  return new ChildFrameCompositingHelper(
      base::WeakPtr<BrowserPlugin>(), render_frame_proxy->web_frame(),
      render_frame_proxy, render_frame_proxy->routing_id());
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
      browser_plugin_(browser_plugin),
      render_frame_proxy_(render_frame_proxy),
      frame_(frame) {}

ChildFrameCompositingHelper::~ChildFrameCompositingHelper() {
  if (resource_collection_.get())
    resource_collection_->SetClient(nullptr);
}

BrowserPluginManager* ChildFrameCompositingHelper::GetBrowserPluginManager() {
  if (!browser_plugin_)
    return nullptr;

  return BrowserPluginManager::Get();
}

blink::WebPluginContainer* ChildFrameCompositingHelper::GetContainer() {
  if (!browser_plugin_)
    return nullptr;

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
        new BrowserPluginHostMsg_CompositorFrameSwappedACK(GetInstanceID(),
                                                           params));
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
        new BrowserPluginHostMsg_ReclaimCompositorResources(GetInstanceID(),
                                                            params));
  } else if (render_frame_proxy_) {
    render_frame_proxy_->Send(
        new FrameHostMsg_ReclaimCompositorResources(host_routing_id_, params));
  }
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

void ChildFrameCompositingHelper::UpdateWebLayer(blink::WebLayer* layer) {
  if (GetContainer()) {
    GetContainer()->setWebLayer(layer);
  } else if (frame_) {
    frame_->setRemoteWebLayer(layer);
  }
  web_layer_.reset(layer);
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
    gfx::Size device_scale_adjusted_size =
        gfx::ScaleToFlooredSize(buffer_size_, 1.0f / device_scale_factor);
    layer->SetBounds(device_scale_adjusted_size);
  }
}

void ChildFrameCompositingHelper::OnContainerDestroy() {
  // If we have a pending ACK, then ACK now so we don't lose frames in the
  // future.
  DidCommitCompositorFrame();

  UpdateWebLayer(nullptr);

  if (resource_collection_.get())
    resource_collection_->SetClient(nullptr);

  ack_pending_ = false;
  resource_collection_ = nullptr;
  frame_provider_ = nullptr;
}

void ChildFrameCompositingHelper::ChildFrameGone() {
  scoped_refptr<cc::SolidColorLayer> crashed_layer =
      cc::SolidColorLayer::Create(cc_blink::WebLayerImpl::LayerSettings());
  crashed_layer->SetMasksToBounds(true);
  crashed_layer->SetBackgroundColor(SkColorSetARGBInline(255, 0, 128, 0));
  blink::WebLayer* layer = new cc_blink::WebLayerImpl(crashed_layer);
  UpdateWebLayer(layer);
}

void ChildFrameCompositingHelper::OnCompositorFrameSwapped(
    scoped_ptr<cc::CompositorFrame> frame,
    int route_id,
    uint32_t output_surface_id,
    int host_id,
    base::SharedMemoryHandle handle) {
  cc::DelegatedFrameData* frame_data = frame->delegated_frame_data.get();

  // Do nothing if we are getting destroyed or have no frame data.
  if (!frame_data)
    return;

  DCHECK(!frame_data->render_pass_list.empty());
  cc::RenderPass* root_pass = frame_data->render_pass_list.back().get();
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
    frame_provider_ = nullptr;

    // Drop the cc::DelegatedFrameResourceCollection so that we will not return
    // any resources from the old output surface with the new output surface id.
    if (resource_collection_.get()) {
      resource_collection_->SetClient(nullptr);

      if (resource_collection_->LoseAllResources())
        SendReturnedDelegatedResources();
      resource_collection_ = nullptr;
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
        resource_collection_.get(), std::move(frame->delegated_frame_data));
    scoped_refptr<cc::DelegatedRendererLayer> delegated_layer =
        cc::DelegatedRendererLayer::Create(
            cc_blink::WebLayerImpl::LayerSettings(), frame_provider_.get());
    delegated_layer->SetIsDrawable(true);
    buffer_size_ = gfx::Size();
    blink::WebLayer* layer = new cc_blink::WebLayerImpl(delegated_layer);
    UpdateWebLayer(layer);
  } else {
    frame_provider_->SetFrameData(std::move(frame->delegated_frame_data));
  }

  CheckSizeAndAdjustLayerProperties(
      frame_data->render_pass_list.back()->output_rect.size(),
      frame->metadata.device_scale_factor,
      static_cast<cc_blink::WebLayerImpl*>(web_layer_.get())->layer());

  ack_pending_ = true;
}

// static
void ChildFrameCompositingHelper::SatisfyCallback(
    scoped_refptr<ThreadSafeSender> sender,
    int host_routing_id,
    cc::SurfaceSequence sequence) {
  // This may be called on either the main or impl thread.
  sender->Send(new FrameHostMsg_SatisfySequence(host_routing_id, sequence));
}

// static
void ChildFrameCompositingHelper::SatisfyCallbackBrowserPlugin(
    scoped_refptr<ThreadSafeSender> sender,
    int host_routing_id,
    int browser_plugin_instance_id,
    cc::SurfaceSequence sequence) {
  sender->Send(new BrowserPluginHostMsg_SatisfySequence(
      host_routing_id, browser_plugin_instance_id, sequence));
}

// static
void ChildFrameCompositingHelper::RequireCallback(
    scoped_refptr<ThreadSafeSender> sender,
    int host_routing_id,
    cc::SurfaceId id,
    cc::SurfaceSequence sequence) {
  // This may be called on either the main or impl thread.
  sender->Send(new FrameHostMsg_RequireSequence(host_routing_id, id, sequence));
}

void ChildFrameCompositingHelper::RequireCallbackBrowserPlugin(
    scoped_refptr<ThreadSafeSender> sender,
    int host_routing_id,
    int browser_plugin_instance_id,
    cc::SurfaceId id,
    cc::SurfaceSequence sequence) {
  // This may be called on either the main or impl thread.
  sender->Send(new BrowserPluginHostMsg_RequireSequence(
      host_routing_id, browser_plugin_instance_id, id, sequence));
}

void ChildFrameCompositingHelper::OnSetSurface(
    const cc::SurfaceId& surface_id,
    const gfx::Size& frame_size,
    float scale_factor,
    const cc::SurfaceSequence& sequence) {
  surface_id_ = surface_id;
  scoped_refptr<ThreadSafeSender> sender(
      RenderThreadImpl::current()->thread_safe_sender());
  cc::SurfaceLayer::SatisfyCallback satisfy_callback =
      render_frame_proxy_
          ? base::Bind(&ChildFrameCompositingHelper::SatisfyCallback, sender,
                       host_routing_id_)
          : base::Bind(
                &ChildFrameCompositingHelper::SatisfyCallbackBrowserPlugin,
                sender, host_routing_id_,
                browser_plugin_->browser_plugin_instance_id());
  cc::SurfaceLayer::RequireCallback require_callback =
      render_frame_proxy_
          ? base::Bind(&ChildFrameCompositingHelper::RequireCallback, sender,
                       host_routing_id_)
          : base::Bind(
                &ChildFrameCompositingHelper::RequireCallbackBrowserPlugin,
                sender, host_routing_id_,
                browser_plugin_->browser_plugin_instance_id());
  scoped_refptr<cc::SurfaceLayer> surface_layer =
      cc::SurfaceLayer::Create(cc_blink::WebLayerImpl::LayerSettings(),
                               satisfy_callback, require_callback);
  // TODO(oshima): This is a stopgap fix so that the compositor does not
  // scaledown the content when 2x frame data is added to 1x parent frame data.
  // Fix this in cc/.
  if (IsUseZoomForDSFEnabled())
    scale_factor = 1.0f;

  surface_layer->SetSurfaceId(surface_id, scale_factor, frame_size);
  surface_layer->SetMasksToBounds(true);
  blink::WebLayer* layer = new cc_blink::WebLayerImpl(surface_layer);
  UpdateWebLayer(layer);

  UpdateVisibility(true);

  // The RWHV creates a destruction dependency on the surface that needs to be
  // satisfied. Note: render_frame_proxy_ is null in the case our client is a
  // BrowserPlugin; in this case the BrowserPlugin sends its own SatisfySequence
  // message.
  if (render_frame_proxy_) {
    render_frame_proxy_->Send(
        new FrameHostMsg_SatisfySequence(host_routing_id_, sequence));
  } else if (browser_plugin_.get()) {
    browser_plugin_->SendSatisfySequence(sequence);
  }

  CheckSizeAndAdjustLayerProperties(
      frame_size, scale_factor,
      static_cast<cc_blink::WebLayerImpl*>(web_layer_.get())->layer());
}

void ChildFrameCompositingHelper::UpdateVisibility(bool visible) {
  if (web_layer_)
    web_layer_->setDrawsContent(visible);
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

}  // namespace content
