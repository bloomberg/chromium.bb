// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/surface_layer.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<SurfaceLayer> SurfaceLayer::Create() {
  return base::WrapRefCounted(new SurfaceLayer());
}

SurfaceLayer::SurfaceLayer() = default;

SurfaceLayer::~SurfaceLayer() {
  DCHECK(!layer_tree_host());
}

void SurfaceLayer::SetPrimarySurfaceId(const viz::SurfaceId& surface_id,
                                       const DeadlinePolicy& deadline_policy) {
  if (primary_surface_id_ == surface_id &&
      deadline_policy.use_existing_deadline()) {
    return;
  }

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Embed.Flow",
      TRACE_ID_GLOBAL(surface_id.local_surface_id().embed_trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SetPrimarySurfaceId", "surface_id", surface_id.ToString());

  const viz::SurfaceRange old_surface_range(GetSurfaceRange());
  if (layer_tree_host() && old_surface_range.IsValid())
    layer_tree_host()->RemoveSurfaceRange(old_surface_range);

  primary_surface_id_ = surface_id;

  const viz::SurfaceRange new_surface_range(GetSurfaceRange());
  if (layer_tree_host() && new_surface_range.IsValid())
    layer_tree_host()->AddSurfaceRange(new_surface_range);

  // We should never block or set a deadline on an invalid
  // |primary_surface_id_|.
  if (!primary_surface_id_.is_valid()) {
    deadline_in_frames_ = 0u;
  } else if (!deadline_policy.use_existing_deadline()) {
    deadline_in_frames_ = deadline_policy.deadline_in_frames();
  }
  UpdateDrawsContent(HasDrawableContent());
  SetNeedsCommit();
}

void SurfaceLayer::SetFallbackSurfaceId(const viz::SurfaceId& surface_id) {
  if (fallback_surface_id_ == surface_id)
    return;

  TRACE_EVENT_WITH_FLOW2(
      TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
      "LocalSurfaceId.Submission.Flow",
      TRACE_ID_GLOBAL(surface_id.local_surface_id().submission_trace_id()),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SetFallbackSurfaceId", "surface_id", surface_id.ToString());

  const viz::SurfaceRange old_surface_range(GetSurfaceRange());
  if (layer_tree_host() && old_surface_range.IsValid())
    layer_tree_host()->RemoveSurfaceRange(old_surface_range);

  fallback_surface_id_ = surface_id;

  const viz::SurfaceRange new_surface_range(GetSurfaceRange());
  if (layer_tree_host() && new_surface_range.IsValid())
    layer_tree_host()->AddSurfaceRange(new_surface_range);

  SetNeedsCommit();
}

void SurfaceLayer::SetStretchContentToFillBounds(
    bool stretch_content_to_fill_bounds) {
  if (stretch_content_to_fill_bounds_ == stretch_content_to_fill_bounds)
    return;
  stretch_content_to_fill_bounds_ = stretch_content_to_fill_bounds;
  SetNeedsPushProperties();
}

void SurfaceLayer::SetSurfaceHitTestable(bool surface_hit_testable) {
  if (surface_hit_testable_ == surface_hit_testable)
    return;
  surface_hit_testable_ = surface_hit_testable;
  SetNeedsPushProperties();
}

std::unique_ptr<LayerImpl> SurfaceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return SurfaceLayerImpl::Create(tree_impl, id());
}

bool SurfaceLayer::HasDrawableContent() const {
  return primary_surface_id_.is_valid() && Layer::HasDrawableContent();
}

void SurfaceLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    return;
  }
  const viz::SurfaceRange old_surface_range(GetSurfaceRange());
  if (layer_tree_host() && old_surface_range.IsValid())
    layer_tree_host()->RemoveSurfaceRange(old_surface_range);

  Layer::SetLayerTreeHost(host);

  const viz::SurfaceRange new_surface_range(GetSurfaceRange());
  if (layer_tree_host() && new_surface_range.IsValid())
    layer_tree_host()->AddSurfaceRange(new_surface_range);
}

void SurfaceLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "SurfaceLayer::PushPropertiesTo");
  SurfaceLayerImpl* layer_impl = static_cast<SurfaceLayerImpl*>(layer);
  layer_impl->SetPrimarySurfaceId(primary_surface_id_,
                                  std::move(deadline_in_frames_));
  // Unless the client explicitly calls SetPrimarySurfaceId again after this
  // commit, don't block on |primary_surface_id_| again.
  deadline_in_frames_ = 0u;
  layer_impl->SetFallbackSurfaceId(fallback_surface_id_);
  layer_impl->SetStretchContentToFillBounds(stretch_content_to_fill_bounds_);
  layer_impl->SetSurfaceHitTestable(surface_hit_testable_);
}

viz::SurfaceRange SurfaceLayer::GetSurfaceRange() const {
  return viz::SurfaceRange(
      fallback_surface_id_.is_valid()
          ? base::Optional<viz::SurfaceId>(fallback_surface_id_)
          : base::nullopt,
      primary_surface_id_.is_valid() ? primary_surface_id_
                                     : fallback_surface_id_);
}

}  // namespace cc
