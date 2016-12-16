// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/output/swap_promise.h"
#include "cc/surfaces/surface_sequence_generator.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/swap_promise_manager.h"

namespace cc {

class SatisfySwapPromise : public SwapPromise {
 public:
  SatisfySwapPromise(
      const SurfaceSequence& sequence,
      const SurfaceLayer::SatisfyCallback& satisfy_callback,
      scoped_refptr<base::SingleThreadTaskRunner> main_task_runner)
      : sequence_(sequence),
        satisfy_callback_(satisfy_callback),
        main_task_runner_(std::move(main_task_runner)) {}

  ~SatisfySwapPromise() override {}

 private:
  void DidActivate() override {}
  void WillSwap(CompositorFrameMetadata* metadata) override {}
  void DidSwap() override {
    // DidSwap could run on compositor thread but satisfy callback must
    // run on the main thread.
    main_task_runner_->PostTask(FROM_HERE,
                                base::Bind(satisfy_callback_, sequence_));
  }

  DidNotSwapAction DidNotSwap(DidNotSwapReason reason) override {
    main_task_runner_->PostTask(FROM_HERE,
                                base::Bind(satisfy_callback_, sequence_));
    return DidNotSwapAction::BREAK_PROMISE;
  }
  int64_t TraceId() const override { return 0; }

  SurfaceSequence sequence_;
  SurfaceLayer::SatisfyCallback satisfy_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(SatisfySwapPromise);
};

scoped_refptr<SurfaceLayer> SurfaceLayer::Create(
    const SatisfyCallback& satisfy_callback,
    const RequireCallback& require_callback) {
  return make_scoped_refptr(
      new SurfaceLayer(satisfy_callback, require_callback));
}

SurfaceLayer::SurfaceLayer(const SatisfyCallback& satisfy_callback,
                           const RequireCallback& require_callback)
    : satisfy_callback_(satisfy_callback),
      require_callback_(require_callback) {}

SurfaceLayer::~SurfaceLayer() {
  DCHECK(!layer_tree_host());
  DCHECK(!destroy_sequence_.is_valid());
}

void SurfaceLayer::SetSurfaceId(const SurfaceId& surface_id,
                                float scale,
                                const gfx::Size& size,
                                bool stretch_content_to_fill_bounds) {
  SatisfyDestroySequence();
  surface_id_ = surface_id;
  surface_size_ = size;
  surface_scale_ = scale;
  stretch_content_to_fill_bounds_ = stretch_content_to_fill_bounds;
  CreateNewDestroySequence();

  UpdateDrawsContent(HasDrawableContent());
  SetNeedsPushProperties();
}

std::unique_ptr<LayerImpl> SurfaceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id());
}

bool SurfaceLayer::HasDrawableContent() const {
  return surface_id_.is_valid() && Layer::HasDrawableContent();
}

void SurfaceLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    Layer::SetLayerTreeHost(host);
    return;
  }

  SatisfyDestroySequence();
  Layer::SetLayerTreeHost(host);
  CreateNewDestroySequence();
}

void SurfaceLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "SurfaceLayer::PushPropertiesTo");
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);

  layer_impl->SetSurfaceId(surface_id_);
  layer_impl->SetSurfaceSize(surface_size_);
  layer_impl->SetSurfaceScale(surface_scale_);
  layer_impl->SetStretchContentToFillBounds(stretch_content_to_fill_bounds_);
}

void SurfaceLayer::CreateNewDestroySequence() {
  DCHECK(!destroy_sequence_.is_valid());
  if (layer_tree_host()) {
    destroy_sequence_ = layer_tree_host()
                            ->GetSurfaceSequenceGenerator()
                            ->CreateSurfaceSequence();
    require_callback_.Run(surface_id_, destroy_sequence_);
  }
}

void SurfaceLayer::SatisfyDestroySequence() {
  if (!layer_tree_host())
    return;
  DCHECK(destroy_sequence_.is_valid());
  auto satisfy =
      base::MakeUnique<SatisfySwapPromise>(destroy_sequence_, satisfy_callback_,
                                           base::ThreadTaskRunnerHandle::Get());
  layer_tree_host()->GetSwapPromiseManager()->QueueSwapPromise(
      std::move(satisfy));
  destroy_sequence_ = SurfaceSequence();
}

}  // namespace cc
