// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/content_layer.h"

#include "base/lazy_instance.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_collections.h"
#include "cc/output/filter_operations.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "content/public/browser/android/compositor.h"
#include "ui/gfx/geometry/size.h"

namespace android {

// static
scoped_refptr<ContentLayer> ContentLayer::Create(
    TabContentManager* tab_content_manager) {
  return make_scoped_refptr(new ContentLayer(tab_content_manager));
}

static void SetOpacityOnLeaf(scoped_refptr<cc::Layer> layer, float alpha) {
  const cc::LayerList& children = layer->children();
  if (children.size() > 0) {
    layer->SetOpacity(1.0f);
    for (uint i = 0; i < children.size(); ++i)
      SetOpacityOnLeaf(children[i], alpha);
  } else {
    layer->SetOpacity(alpha);
  }
}

static bool DoesLeafDrawContents(scoped_refptr<cc::Layer> layer) {
  if (!layer.get())
    return false;

  // If the subtree is hidden, then any layers in this tree will not be drawn.
  if (layer->hide_layer_and_subtree())
    return false;

  // TODO: Remove the need for this logic. We can't really guess from
  // an opaque layer type whether it has valid live contents, or for example
  // just a background color placeholder. Need to get this from somewhere else
  // like ContentViewCore or RWHV.
  if (layer->DrawsContent() && !layer->background_color()) {
    return true;
  }

  const cc::LayerList& children = layer->children();
  for (unsigned i = 0; i < children.size(); i++) {
    if (DoesLeafDrawContents(children[i]))
      return true;
  }
  return false;
}

void ContentLayer::SetProperties(int id,
                                 bool can_use_live_layer,
                                 float static_to_view_blend,
                                 bool should_override_content_alpha,
                                 float content_alpha_override,
                                 float saturation,
                                 bool should_clip,
                                 const gfx::Rect& clip) {
  scoped_refptr<cc::Layer> content_layer =
      tab_content_manager_->GetLiveLayer(id);
  bool content_layer_draws = DoesLeafDrawContents(content_layer);

  scoped_refptr<ThumbnailLayer> static_layer =
      tab_content_manager_->GetStaticLayer(
          id, !(can_use_live_layer && content_layer_draws));

  float content_opacity =
      should_override_content_alpha ? content_alpha_override : 1.0f;
  float static_opacity =
      should_override_content_alpha ? content_alpha_override : 1.0f;
  if (content_layer.get() && can_use_live_layer && content_layer_draws)
    static_opacity = static_to_view_blend;
  if (!can_use_live_layer)
    content_opacity = 0.0f;

  const cc::LayerList& layer_children = layer_->children();
  for (unsigned i = 0; i < layer_children.size(); i++)
    layer_children[i]->RemoveFromParent();

  if (content_layer.get()) {
    content_layer->SetMasksToBounds(should_clip);
    content_layer->SetBounds(clip.size());
    SetOpacityOnLeaf(content_layer, content_opacity);

    layer_->AddChild(content_layer);
  }
  if (static_layer.get()) {
    static_layer->layer()->SetIsDrawable(true);
    if (should_clip)
      static_layer->Clip(clip);
    else
      static_layer->ClearClip();
    SetOpacityOnLeaf(static_layer->layer(), static_opacity);

    cc::FilterOperations static_filter_operations;
    if (saturation < 1.0f) {
      static_filter_operations.Append(
          cc::FilterOperation::CreateSaturateFilter(saturation));
    }
    static_layer->layer()->SetFilters(static_filter_operations);

    layer_->AddChild(static_layer->layer());
  }
}

scoped_refptr<cc::Layer> ContentLayer::layer() {
  return layer_;
}

ContentLayer::ContentLayer(TabContentManager* tab_content_manager)
    : layer_(cc::Layer::Create()),
      tab_content_manager_(tab_content_manager) {}

ContentLayer::~ContentLayer() {
}

}  //  namespace android
