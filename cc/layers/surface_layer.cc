// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/swap_promise.h"
#include "cc/trees/swap_promise_manager.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/surfaces/surface_sequence_generator.h"

namespace cc {

class SatisfySwapPromise : public SwapPromise {
 public:
  SatisfySwapPromise(
      base::Closure reference_returner,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : reference_returner_(reference_returner),
        main_task_runner_(std::move(main_task_runner)) {}

  ~SatisfySwapPromise() override {}

 private:
  void DidActivate() override {}

  void WillSwap(viz::CompositorFrameMetadata* metadata) override {}

  void DidSwap() override {
    main_task_runner_->PostTask(FROM_HERE, reference_returner_);
  }

  DidNotSwapAction DidNotSwap(DidNotSwapReason reason) override {
    main_task_runner_->PostTask(FROM_HERE, reference_returner_);
    return DidNotSwapAction::BREAK_PROMISE;
  }

  int64_t TraceId() const override { return 0; }

  base::Closure reference_returner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SatisfySwapPromise);
};

scoped_refptr<SurfaceLayer> SurfaceLayer::Create(
    scoped_refptr<viz::SurfaceReferenceFactory> ref_factory) {
  return base::WrapRefCounted(new SurfaceLayer(std::move(ref_factory)));
}

SurfaceLayer::SurfaceLayer(
    scoped_refptr<viz::SurfaceReferenceFactory> ref_factory)
    : ref_factory_(std::move(ref_factory)) {}

SurfaceLayer::~SurfaceLayer() {
  DCHECK(!layer_tree_host());
}

void SurfaceLayer::SetPrimarySurfaceInfo(const viz::SurfaceInfo& surface_info) {
  primary_surface_info_ = surface_info;
  UpdateDrawsContent(HasDrawableContent());
  SetNeedsCommit();
}

void SurfaceLayer::SetFallbackSurfaceInfo(
    const viz::SurfaceInfo& surface_info) {
  RemoveReference(std::move(fallback_reference_returner_));
  if (layer_tree_host())
    layer_tree_host()->RemoveSurfaceLayerId(fallback_surface_info_.id());

  fallback_surface_info_ = surface_info;

  if (layer_tree_host() && fallback_surface_info_.is_valid()) {
    fallback_reference_returner_ = ref_factory_->CreateReference(
        layer_tree_host(), fallback_surface_info_.id());
    layer_tree_host()->AddSurfaceLayerId(fallback_surface_info_.id());
  }
  SetNeedsCommit();
}

void SurfaceLayer::SetDefaultBackgroundColor(SkColor background_color) {
  default_background_color_ = background_color;
  SetNeedsPushProperties();
}

void SurfaceLayer::SetStretchContentToFillBounds(
    bool stretch_content_to_fill_bounds) {
  stretch_content_to_fill_bounds_ = stretch_content_to_fill_bounds;
  SetNeedsPushProperties();
}

std::unique_ptr<LayerImpl> SurfaceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id());
}

bool SurfaceLayer::HasDrawableContent() const {
  return primary_surface_info_.is_valid() && Layer::HasDrawableContent();
}

void SurfaceLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    return;
  }

  if (layer_tree_host() && fallback_surface_info_.is_valid())
    layer_tree_host()->RemoveSurfaceLayerId(fallback_surface_info_.id());

  RemoveReference(std::move(fallback_reference_returner_));
  Layer::SetLayerTreeHost(host);

  if (layer_tree_host() && fallback_surface_info_.is_valid()) {
    fallback_reference_returner_ = ref_factory_->CreateReference(
        layer_tree_host(), fallback_surface_info_.id());
    layer_tree_host()->AddSurfaceLayerId(fallback_surface_info_.id());
  }
}

void SurfaceLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "SurfaceLayer::PushPropertiesTo");
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);
  layer_impl->SetPrimarySurfaceInfo(primary_surface_info_);
  layer_impl->SetFallbackSurfaceInfo(fallback_surface_info_);
  layer_impl->SetStretchContentToFillBounds(stretch_content_to_fill_bounds_);
  layer_impl->SetDefaultBackgroundColor(default_background_color_);
}

void SurfaceLayer::RemoveReference(base::Closure reference_returner) {
  if (!reference_returner)
    return;
  auto swap_promise = std::make_unique<SatisfySwapPromise>(
      std::move(reference_returner),
      layer_tree_host()->GetTaskRunnerProvider()->MainThreadTaskRunner());
  layer_tree_host()->GetSwapPromiseManager()->QueueSwapPromise(
      std::move(swap_promise));
}

}  // namespace cc
