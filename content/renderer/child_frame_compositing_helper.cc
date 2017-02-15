// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/child_frame_compositing_helper.h"

#include <utility>

#include "cc/blink/web_layer_impl.h"
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/output/context_provider.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "cc/resources/single_release_callback.h"
#include "cc/surfaces/sequence_surface_reference_factory.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/content_switches_internal.h"
#include "content/common/frame_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_frame_proxy.h"
#include "content/renderer/render_thread_impl.h"
#include "services/ui/public/cpp/gpu/context_provider_command_buffer.h"
#include "skia/ext/image_operations.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebRemoteFrame.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/skia_util.h"

namespace content {

namespace {

class IframeSurfaceReferenceFactory
    : public cc::SequenceSurfaceReferenceFactory {
 public:
  IframeSurfaceReferenceFactory(scoped_refptr<ThreadSafeSender> sender,
                                int routing_id)
      : sender_(std::move(sender)), routing_id_(routing_id) {}

 private:
  ~IframeSurfaceReferenceFactory() override = default;

  // cc::SequenceSurfaceReferenceFactory implementation:
  void RequireSequence(const cc::SurfaceId& surface_id,
                       const cc::SurfaceSequence& sequence) const override {
    sender_->Send(
        new FrameHostMsg_RequireSequence(routing_id_, surface_id, sequence));
  }

  void SatisfySequence(const cc::SurfaceSequence& sequence) const override {
    sender_->Send(new FrameHostMsg_SatisfySequence(routing_id_, sequence));
  }

  const scoped_refptr<ThreadSafeSender> sender_;
  const int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(IframeSurfaceReferenceFactory);
};

class BrowserPluginSurfaceReferenceFactory
    : public cc::SequenceSurfaceReferenceFactory {
 public:
  BrowserPluginSurfaceReferenceFactory(scoped_refptr<ThreadSafeSender> sender,
                                       int routing_id,
                                       int browser_plugin_instance_id)
      : sender_(std::move(sender)),
        routing_id_(routing_id),
        browser_plugin_instance_id_(browser_plugin_instance_id) {}

 private:
  ~BrowserPluginSurfaceReferenceFactory() override = default;

  // cc::SequenceSurfaceRefrenceFactory implementation:
  void SatisfySequence(const cc::SurfaceSequence& seq) const override {
    sender_->Send(new BrowserPluginHostMsg_SatisfySequence(
        routing_id_, browser_plugin_instance_id_, seq));
  }

  void RequireSequence(const cc::SurfaceId& surface_id,
                       const cc::SurfaceSequence& sequence) const override {
    sender_->Send(new BrowserPluginHostMsg_RequireSequence(
        routing_id_, browser_plugin_instance_id_, surface_id, sequence));
  }

  const scoped_refptr<ThreadSafeSender> sender_;
  const int routing_id_;
  const int browser_plugin_instance_id_;

  DISALLOW_COPY_AND_ASSIGN(BrowserPluginSurfaceReferenceFactory);
};

}  // namespace

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
    blink::WebRemoteFrame* frame,
    RenderFrameProxy* render_frame_proxy,
    int host_routing_id)
    : host_routing_id_(host_routing_id),
      browser_plugin_(browser_plugin),
      render_frame_proxy_(render_frame_proxy),
      frame_(frame) {
  scoped_refptr<ThreadSafeSender> sender(
      RenderThreadImpl::current()->thread_safe_sender());
  if (render_frame_proxy_) {
    surface_reference_factory_ =
        new IframeSurfaceReferenceFactory(sender, host_routing_id_);
  } else {
    surface_reference_factory_ = new BrowserPluginSurfaceReferenceFactory(
        sender, host_routing_id_,
        browser_plugin_->browser_plugin_instance_id());
  }
}

ChildFrameCompositingHelper::~ChildFrameCompositingHelper() {
}

blink::WebPluginContainer* ChildFrameCompositingHelper::GetContainer() {
  if (!browser_plugin_)
    return nullptr;

  return browser_plugin_->container();
}

void ChildFrameCompositingHelper::UpdateWebLayer(
    std::unique_ptr<blink::WebLayer> layer) {
  if (GetContainer()) {
    GetContainer()->setWebLayer(layer.get());
  } else if (frame_) {
    frame_->setWebLayer(layer.get());
  }
  web_layer_ = std::move(layer);
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
  UpdateWebLayer(nullptr);
}

void ChildFrameCompositingHelper::ChildFrameGone() {
  scoped_refptr<cc::SolidColorLayer> crashed_layer =
      cc::SolidColorLayer::Create();
  crashed_layer->SetMasksToBounds(true);
  crashed_layer->SetBackgroundColor(SK_ColorBLACK);

  if (web_layer_) {
    SkBitmap* sad_bitmap =
        GetContentClient()->renderer()->GetSadWebViewBitmap();
    if (sad_bitmap && web_layer_->bounds().width > sad_bitmap->width() &&
        web_layer_->bounds().height > sad_bitmap->height()) {
      scoped_refptr<cc::PictureImageLayer> sad_layer =
          cc::PictureImageLayer::Create();
      sad_layer->SetImage(SkImage::MakeFromBitmap(*sad_bitmap));
      sad_layer->SetBounds(
          gfx::Size(sad_bitmap->width(), sad_bitmap->height()));
      sad_layer->SetPosition(gfx::PointF(
          (web_layer_->bounds().width - sad_bitmap->width()) / 2,
          (web_layer_->bounds().height - sad_bitmap->height()) / 2));
      sad_layer->SetIsDrawable(true);

      crashed_layer->AddChild(sad_layer);
    }
  }

  std::unique_ptr<blink::WebLayer> layer(
      new cc_blink::WebLayerImpl(crashed_layer));
  UpdateWebLayer(std::move(layer));
}

void ChildFrameCompositingHelper::OnSetSurface(
    const cc::SurfaceInfo& surface_info,
    const cc::SurfaceSequence& sequence) {
  float scale_factor = surface_info.device_scale_factor();
  surface_id_ = surface_info.id();
  scoped_refptr<cc::SurfaceLayer> surface_layer =
      cc::SurfaceLayer::Create(surface_reference_factory_);
  // TODO(oshima): This is a stopgap fix so that the compositor does not
  // scaledown the content when 2x frame data is added to 1x parent frame data.
  // Fix this in cc/.
  if (IsUseZoomForDSFEnabled())
    scale_factor = 1.0f;

  surface_layer->SetPrimarySurfaceInfo(cc::SurfaceInfo(
      surface_info.id(), scale_factor, surface_info.size_in_pixels()));
  surface_layer->SetMasksToBounds(true);
  std::unique_ptr<cc_blink::WebLayerImpl> layer(
      new cc_blink::WebLayerImpl(surface_layer));
  // TODO(lfg): Investigate if it's possible to propagate the information about
  // the child surface's opacity. https://crbug.com/629851.
  layer->setOpaque(false);
  layer->SetContentsOpaqueIsFixed(true);
  UpdateWebLayer(std::move(layer));

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
      surface_info.size_in_pixels(), surface_info.device_scale_factor(),
      static_cast<cc_blink::WebLayerImpl*>(web_layer_.get())->layer());
}

void ChildFrameCompositingHelper::UpdateVisibility(bool visible) {
  if (web_layer_)
    web_layer_->setDrawsContent(visible);
}

}  // namespace content
