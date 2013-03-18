// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/delegated_renderer_layer.h"

#include "cc/layers/delegated_renderer_layer_impl.h"
#include "cc/output/delegated_frame_data.h"

namespace cc {

scoped_refptr<DelegatedRendererLayer> DelegatedRendererLayer::Create() {
  return scoped_refptr<DelegatedRendererLayer>(new DelegatedRendererLayer());
}

DelegatedRendererLayer::DelegatedRendererLayer()
    : Layer() {
}

DelegatedRendererLayer::~DelegatedRendererLayer() {}

scoped_ptr<LayerImpl> DelegatedRendererLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return DelegatedRendererLayerImpl::Create(
      tree_impl, layer_id_).PassAs<LayerImpl>();
}

bool DelegatedRendererLayer::DrawsContent() const {
  return Layer::DrawsContent() && !frame_size_.IsEmpty();
}

void DelegatedRendererLayer::PushPropertiesTo(LayerImpl* impl) {
  Layer::PushPropertiesTo(impl);

  DelegatedRendererLayerImpl* delegated_impl =
      static_cast<DelegatedRendererLayerImpl*>(impl);

  delegated_impl->SetDisplaySize(display_size_);

  if (!frame_data_) {
    delegated_impl->SetFrameData(scoped_ptr<DelegatedFrameData>(),
                                 gfx::Rect(),
                                 &unused_resources_for_child_compositor_);
  } else if (frame_size_.IsEmpty()) {
    scoped_ptr<DelegatedFrameData> empty_frame(new DelegatedFrameData);
    delegated_impl->SetFrameData(empty_frame.Pass(),
                                 gfx::Rect(),
                                 &unused_resources_for_child_compositor_);
  } else {
    delegated_impl->SetFrameData(frame_data_.Pass(),
                                 damage_in_frame_,
                                 &unused_resources_for_child_compositor_);
  }
  frame_data_.reset();
  damage_in_frame_ = gfx::RectF();
}

void DelegatedRendererLayer::SetDisplaySize(gfx::Size size) {
  if (display_size_ == size)
    return;
  display_size_ = size;
  SetNeedsCommit();
}

void DelegatedRendererLayer::SetFrameData(
    scoped_ptr<DelegatedFrameData> new_frame_data) {
  if (frame_data_) {
    // Copy the resources from the last provided frame into the new frame, as
    // it may use resources that were transferred in the last frame.
    new_frame_data->resource_list.insert(new_frame_data->resource_list.end(),
                                         frame_data_->resource_list.begin(),
                                         frame_data_->resource_list.end());
  }
  frame_data_ = new_frame_data.Pass();
  if (!frame_data_->render_pass_list.empty()) {
    RenderPass* root_pass = frame_data_->render_pass_list.back();
    damage_in_frame_.Union(root_pass->damage_rect);
    frame_size_ = root_pass->output_rect.size();

    // TODO(danakj): This could be optimized to only add resources to the
    // frame_data_ if they are actually used in the frame. For now, it will
    // cause the parent (this layer) to hold onto some resources it doesn't
    // need to for an extra frame.
    for (size_t i = 0; i < unused_resources_for_child_compositor_.size(); ++i) {
      frame_data_->resource_list.push_back(
          unused_resources_for_child_compositor_[i]);
    }
    unused_resources_for_child_compositor_.clear();
  } else {
    frame_size_ = gfx::Size();
  }
  SetNeedsCommit();
}

void DelegatedRendererLayer::TakeUnusedResourcesForChildCompositor(
    TransferableResourceArray* array) {
  DCHECK(array->empty());
  array->clear();

  array->swap(unused_resources_for_child_compositor_);
}

}  // namespace cc
