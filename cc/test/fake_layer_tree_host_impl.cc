// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_layer_tree_host_impl.h"
#include "cc/test/test_shared_bitmap_manager.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(Proxy* proxy,
                                             SharedBitmapManager* manager)
    : LayerTreeHostImpl(LayerTreeSettings(),
                        &client_,
                        proxy,
                        &stats_instrumentation_,
                        manager,
                        0) {
  // Explicitly clear all debug settings.
  SetDebugState(LayerTreeDebugState());
  SetViewportSize(gfx::Size(100, 100));

  // Avoid using Now() as the frame time in unit tests.
  base::TimeTicks time_ticks = base::TimeTicks::FromInternalValue(1);
  SetCurrentFrameTimeTicks(time_ticks);
}

FakeLayerTreeHostImpl::FakeLayerTreeHostImpl(const LayerTreeSettings& settings,
                                             Proxy* proxy,
                                             SharedBitmapManager* manager)
    : LayerTreeHostImpl(settings,
                        &client_,
                        proxy,
                        &stats_instrumentation_,
                        manager,
                        0) {
  // Explicitly clear all debug settings.
  SetDebugState(LayerTreeDebugState());

  // Avoid using Now() as the frame time in unit tests.
  base::TimeTicks time_ticks = base::TimeTicks::FromInternalValue(1);
  SetCurrentFrameTimeTicks(time_ticks);
}

FakeLayerTreeHostImpl::~FakeLayerTreeHostImpl() {}

void FakeLayerTreeHostImpl::CreatePendingTree() {
  LayerTreeHostImpl::CreatePendingTree();
  float arbitrary_large_page_scale = 100000.f;
  pending_tree()->SetPageScaleFactorAndLimits(
      1.f, 1.f / arbitrary_large_page_scale, arbitrary_large_page_scale);
}

base::TimeTicks FakeLayerTreeHostImpl::CurrentFrameTimeTicks() {
  if (current_frame_time_ticks_.is_null())
    return LayerTreeHostImpl::CurrentFrameTimeTicks();
  return current_frame_time_ticks_;
}

void FakeLayerTreeHostImpl::SetCurrentFrameTimeTicks(
    base::TimeTicks current_frame_time_ticks) {
  current_frame_time_ticks_ = current_frame_time_ticks;
}

int FakeLayerTreeHostImpl::RecursiveUpdateNumChildren(LayerImpl* layer) {
  int num_children_that_draw_content = 0;
  for (size_t i = 0; i < layer->children().size(); ++i) {
    num_children_that_draw_content +=
        RecursiveUpdateNumChildren(layer->children()[i]);
  }
  if (layer->DrawsContent() && layer->HasDelegatedContent())
    num_children_that_draw_content += 1000;
  layer->SetNumDescendantsThatDrawContent(num_children_that_draw_content);
  return num_children_that_draw_content + (layer->DrawsContent() ? 1 : 0);
}

void FakeLayerTreeHostImpl::UpdateNumChildrenAndDrawPropertiesForActiveTree() {
  UpdateNumChildrenAndDrawProperties(active_tree());
}

void FakeLayerTreeHostImpl::UpdateNumChildrenAndDrawProperties(
    LayerTreeImpl* layerTree) {
  RecursiveUpdateNumChildren(layerTree->root_layer());
  layerTree->UpdateDrawProperties();
}

}  // namespace cc
