// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/child_frame_compositing_helper.h"

#include <utility>

#include "base/command_line.h"
#include "build/build_config.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/surfaces/sequence_surface_reference_factory.h"
#include "components/viz/common/surfaces/stub_surface_reference_factory.h"
#include "components/viz/common/switches.h"
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

bool AreSurfaceReferencesEnabled() {
#if defined(OS_ANDROID)
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableSurfaceReferences);
#else
  // Surface references are always enabled for non-Android platforms.
  return true;
#endif
}

class IframeSurfaceReferenceFactory
    : public viz::SequenceSurfaceReferenceFactory {
 public:
  IframeSurfaceReferenceFactory(scoped_refptr<ThreadSafeSender> sender,
                                int routing_id)
      : sender_(std::move(sender)), routing_id_(routing_id) {}

  void AddPendingSequence(const viz::SurfaceSequence& sequence) {
    ReleasePendingSequenceIfNecessary();
    pending_sequence_ = sequence;
  }

 private:
  ~IframeSurfaceReferenceFactory() override {
    ReleasePendingSequenceIfNecessary();
  }

  void ReleasePendingSequenceIfNecessary() const {
    if (pending_sequence_.is_valid()) {
      sender_->Send(
          new FrameHostMsg_SatisfySequence(routing_id_, pending_sequence_));
      pending_sequence_ = viz::SurfaceSequence();
    }
  }

  // cc::SequenceSurfaceReferenceFactory implementation:
  void RequireSequence(const viz::SurfaceId& surface_id,
                       const viz::SurfaceSequence& sequence) const override {
    sender_->Send(
        new FrameHostMsg_RequireSequence(routing_id_, surface_id, sequence));
    // If there is a temporary reference that was waiting on a new one to be
    // created, it is now safe to release it.
    ReleasePendingSequenceIfNecessary();
  }

  void SatisfySequence(const viz::SurfaceSequence& sequence) const override {
    sender_->Send(new FrameHostMsg_SatisfySequence(routing_id_, sequence));
  }

  const scoped_refptr<ThreadSafeSender> sender_;
  mutable viz::SurfaceSequence pending_sequence_;
  const int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(IframeSurfaceReferenceFactory);
};

class BrowserPluginSurfaceReferenceFactory
    : public viz::SequenceSurfaceReferenceFactory {
 public:
  BrowserPluginSurfaceReferenceFactory(scoped_refptr<ThreadSafeSender> sender,
                                       int routing_id,
                                       int browser_plugin_instance_id)
      : sender_(std::move(sender)),
        routing_id_(routing_id),
        browser_plugin_instance_id_(browser_plugin_instance_id) {}

  void AddPendingSequence(const viz::SurfaceSequence& sequence) {
    ReleasePendingSequenceIfNecessary();
    pending_sequence_ = sequence;
  }

 private:
  ~BrowserPluginSurfaceReferenceFactory() override {
    ReleasePendingSequenceIfNecessary();
  }

  void ReleasePendingSequenceIfNecessary() const {
    if (pending_sequence_.is_valid()) {
      sender_->Send(new BrowserPluginHostMsg_SatisfySequence(
          routing_id_, browser_plugin_instance_id_, pending_sequence_));
      pending_sequence_ = viz::SurfaceSequence();
    }
  }

  // cc::SequenceSurfaceRefrenceFactory implementation:
  void SatisfySequence(const viz::SurfaceSequence& seq) const override {
    sender_->Send(new BrowserPluginHostMsg_SatisfySequence(
        routing_id_, browser_plugin_instance_id_, seq));
  }

  void RequireSequence(const viz::SurfaceId& surface_id,
                       const viz::SurfaceSequence& sequence) const override {
    sender_->Send(new BrowserPluginHostMsg_RequireSequence(
        routing_id_, browser_plugin_instance_id_, surface_id, sequence));
    // If there is a temporary reference that was waiting on a new one to be
    // created, it is now safe to release it.
    ReleasePendingSequenceIfNecessary();
  }

  const scoped_refptr<ThreadSafeSender> sender_;
  mutable viz::SurfaceSequence pending_sequence_;
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
      frame_(frame),
      enable_surface_references_(AreSurfaceReferencesEnabled()) {
  // In some tests there is no RenderThreadImpl instance.
  if (enable_surface_references_ || !RenderThreadImpl::current()) {
    surface_reference_factory_ = new viz::StubSurfaceReferenceFactory();
  } else {
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
}

ChildFrameCompositingHelper::~ChildFrameCompositingHelper() {
}

blink::WebPluginContainer* ChildFrameCompositingHelper::GetContainer() {
  if (!browser_plugin_)
    return nullptr;

  return browser_plugin_->Container();
}

void ChildFrameCompositingHelper::UpdateWebLayer(
    std::unique_ptr<blink::WebLayer> layer) {
  if (GetContainer()) {
    GetContainer()->SetWebLayer(layer.get());
  } else if (frame_) {
    frame_->SetWebLayer(layer.get());
  }
  web_layer_ = std::move(layer);
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
    if (sad_bitmap && web_layer_->Bounds().width > sad_bitmap->width() &&
        web_layer_->Bounds().height > sad_bitmap->height()) {
      scoped_refptr<cc::PictureImageLayer> sad_layer =
          cc::PictureImageLayer::Create();
      sad_layer->SetImage(cc::PaintImageBuilder::WithDefault()
                              .set_id(cc::PaintImage::kNonLazyStableId)
                              .set_image(SkImage::MakeFromBitmap(*sad_bitmap))
                              .TakePaintImage());
      sad_layer->SetBounds(
          gfx::Size(sad_bitmap->width(), sad_bitmap->height()));
      sad_layer->SetPosition(gfx::PointF(
          (web_layer_->Bounds().width - sad_bitmap->width()) / 2,
          (web_layer_->Bounds().height - sad_bitmap->height()) / 2));
      sad_layer->SetIsDrawable(true);

      crashed_layer->AddChild(sad_layer);
    }
  }

  std::unique_ptr<blink::WebLayer> layer(
      new cc_blink::WebLayerImpl(crashed_layer));
  UpdateWebLayer(std::move(layer));
}

void ChildFrameCompositingHelper::SetPrimarySurfaceId(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_dip) {
  if (last_primary_surface_id_ == surface_id)
    return;

  last_primary_surface_id_ = surface_id;

  surface_layer_ = cc::SurfaceLayer::Create(surface_reference_factory_);
  surface_layer_->SetMasksToBounds(true);
  surface_layer_->SetBackgroundColor(SK_ColorTRANSPARENT);

  surface_layer_->SetPrimarySurfaceId(surface_id, base::nullopt);
  surface_layer_->SetFallbackSurfaceId(fallback_surface_id_);

  std::unique_ptr<cc_blink::WebLayerImpl> layer(
      new cc_blink::WebLayerImpl(surface_layer_));
  // TODO(lfg): Investigate if it's possible to propagate the information about
  // the child surface's opacity. https://crbug.com/629851.
  layer->SetOpaque(false);
  layer->SetContentsOpaqueIsFixed(true);
  UpdateWebLayer(std::move(layer));

  UpdateVisibility(true);

  static_cast<cc_blink::WebLayerImpl*>(web_layer_.get())
      ->layer()
      ->SetBounds(frame_size_in_dip);
}

void ChildFrameCompositingHelper::SetFallbackSurfaceId(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_dip,
    const viz::SurfaceSequence& sequence) {
  if (fallback_surface_id_ == surface_id)
    return;

  fallback_surface_id_ = surface_id;
  // The RWHV creates a destruction dependency on the surface that needs to be
  // satisfied. The reference factory will satisfy it when a new reference has
  // been created.
  if (!enable_surface_references_) {
    if (render_frame_proxy_) {
      static_cast<IframeSurfaceReferenceFactory*>(
          surface_reference_factory_.get())
          ->AddPendingSequence(sequence);
    } else {
      static_cast<BrowserPluginSurfaceReferenceFactory*>(
          surface_reference_factory_.get())
          ->AddPendingSequence(sequence);
    }
  }

  if (!surface_layer_) {
    SetPrimarySurfaceId(surface_id, frame_size_in_dip);
    return;
  }

  surface_layer_->SetFallbackSurfaceId(surface_id);
}

void ChildFrameCompositingHelper::UpdateVisibility(bool visible) {
  if (web_layer_)
    web_layer_->SetDrawsContent(visible);
}

}  // namespace content
