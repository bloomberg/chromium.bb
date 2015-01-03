// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/content_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/layer_lists.h"
#include "chrome/browser/android/compositor/layer/thumbnail_layer.h"
#include "chrome/browser/android/compositor/tab_content_manager.h"
#include "ui/gfx/geometry/size.h"

namespace chrome {
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

  // TODO: Remove the need for this logic. We can't really guess from
  // an opaque layer type whether it has valid live contents, or for example
  // just a background color placeholder. Need to get this from somewhere else
  // like ContentViewCore or RWHV.
  if (layer->DrawsContent() && !layer->hide_layer_and_subtree() &&
      !layer->background_color()) {
    return true;
  }

  const cc::LayerList& children = layer->children();
  for (unsigned i = 0; i < children.size(); i++) {
    if (DoesLeafDrawContents(children[i]))
      return true;
  }
  return false;
}

static gfx::Size GetLeafBounds(scoped_refptr<cc::Layer> layer) {
  if (layer->children().size() > 0)
    return GetLeafBounds(layer->children()[0]);
  return layer->bounds();
}

void ContentLayer::SetProperties(int id,
                                 bool can_use_live_layer,
                                 bool can_use_ntp_fallback,
                                 float static_to_view_blend,
                                 bool should_override_content_alpha,
                                 float content_alpha_override,
                                 float saturation,
                                 const gfx::Rect& desired_bounds,
                                 const gfx::Size& content_size) {
  scoped_refptr<cc::Layer> content_layer =
      tab_content_manager_->GetLiveLayer(id);
  ClipContentLayer(content_layer, desired_bounds, content_size);
  bool content_layer_draws = DoesLeafDrawContents(content_layer);

  scoped_refptr<ThumbnailLayer> static_layer =
      tab_content_manager_->GetStaticLayer(id, !content_layer_draws);

  ClipStaticLayer(static_layer, desired_bounds);

  // Reset the attachment logic if the number of children doesn't match the
  // boolean flags. At some point while a tab is in the background one or more
  // layers may be removed from this tree.
  // Note that this needs to be checked *after* we access TabContentManager, as
  // that class might remove layers internally, messing up our own tracking.
  unsigned int expected_layers = 0;
  expected_layers += content_attached_ ? 1 : 0;
  expected_layers += static_attached_ ? 1 : 0;
  if (layer_->children().size() != expected_layers) {
    content_attached_ = false;
    static_attached_ = false;
    const cc::LayerList& layer_children = layer_->children();
    for (unsigned i = 0; i < layer_children.size(); i++)
      layer_children[i]->RemoveFromParent();
  }

  gfx::Size content_bounds(0, 0);
  if (!content_layer.get() || !can_use_live_layer) {
    SetContentLayer(nullptr);
    SetStaticLayer(static_layer);
    if (static_layer.get())
      content_bounds = static_layer->layer()->bounds();
    else
      content_bounds.set_width(content_size.width());
  } else {
    SetContentLayer(content_layer);
    content_bounds = content_layer->bounds();

    if (static_to_view_blend == 0.0f && !content_layer_draws)
      static_to_view_blend = 1.0f;

    if (static_to_view_blend != 0.0f && static_layer.get()) {
      static_layer->layer()->SetOpacity(static_to_view_blend);
      SetStaticLayer(static_layer);
      if (content_bounds.GetArea() == 0 || !content_layer_draws)
        content_bounds = static_layer->layer()->bounds();
    } else {
      SetStaticLayer(nullptr);
    }
  }

  if (should_override_content_alpha) {
    for (unsigned int i = 0; i < layer_->children().size(); ++i)
      SetOpacityOnLeaf(layer_->children()[i], content_alpha_override);
  }

  if (!content_layer_draws && !static_attached_)
    content_bounds = gfx::Size(0, 0);

  layer_->SetBounds(content_bounds);

  // Only worry about saturation on the static layer
  if (static_layer.get()) {
    if (saturation != saturation_) {
      saturation_ = saturation;
      cc::FilterOperations filters;
      if (saturation_ < 1.0f)
        filters.Append(cc::FilterOperation::CreateSaturateFilter(saturation_));
      static_layer->layer()->SetFilters(filters);
    }
  }
}

gfx::Size ContentLayer::GetContentSize() {
  if (content_attached_ && DoesLeafDrawContents(layer()->children()[0]))
    return layer_->children()[0]->bounds();
  return gfx::Size(0, 0);
}

scoped_refptr<cc::Layer> ContentLayer::layer() {
  return layer_;
}

ContentLayer::ContentLayer(TabContentManager* tab_content_manager)
    : layer_(cc::Layer::Create()),
      content_attached_(false),
      static_attached_(false),
      saturation_(1.0f),
      tab_content_manager_(tab_content_manager) {
}

ContentLayer::~ContentLayer() {
}

void ContentLayer::SetContentLayer(scoped_refptr<cc::Layer> layer) {
  // Check indices
  // content_attached_, expect at least 1 child.
  DCHECK(!content_attached_ || layer_->children().size() > 0);

  if (!layer.get()) {
    if (content_attached_)
      layer_->child_at(0)->RemoveFromParent();
    content_attached_ = false;
    return;
  }

  bool new_layer = false;
  if (content_attached_ && layer_->child_at(0)->id() != layer->id()) {
    layer_->ReplaceChild(layer_->child_at(0), layer);
    new_layer = true;
  } else if (!content_attached_) {
    layer_->InsertChild(layer, 0);
    new_layer = true;
  }

  // If this is a new layer, reset it's opacity.
  if (new_layer)
    SetOpacityOnLeaf(layer, 1.0f);

  content_attached_ = true;
}

void ContentLayer::SetStaticLayer(
    scoped_refptr<ThumbnailLayer> new_static_layer) {
  // Make sure child access will be valid.
  // !content_attached_ AND !static_attached_, expect 0 children.
  // content_attached_ XOR static_attached_, expect 1 child.
  // content_attached_ AND static_attached_, expect 2 children.
  DCHECK((!content_attached_ && !static_attached_) ||
         (content_attached_ != static_attached_ &&
          layer_->children().size() >= 1) ||
         (content_attached_ && static_attached_ &&
          layer_->children().size() >= 2));

  if (!new_static_layer.get()) {
    if (static_layer_.get()) {
      static_layer_->layer()->RemoveFromParent();
      static_layer_ = nullptr;
    }
    static_attached_ = false;
    return;
  }
  static_layer_ = new_static_layer;
  static_layer_->AddSelfToParentOrReplaceAt(layer_, content_attached_ ? 1 : 0);
  saturation_ = -1.0f;
  static_layer_->layer()->SetIsDrawable(true);
  static_attached_ = true;
}

void ContentLayer::ClipContentLayer(scoped_refptr<cc::Layer> content_layer,
                                    gfx::Rect clipping,
                                    gfx::Size content_size) {
  if (!content_layer.get())
    return;

  gfx::Size bounds(GetLeafBounds(content_layer));
  content_layer->SetMasksToBounds(true);
  gfx::Size clamped_bounds(bounds);
  clamped_bounds.SetToMin(clipping.size());
  content_layer->SetBounds(clamped_bounds);

  if (content_layer->children().size() > 0) {
    gfx::PointF offset(
        std::min(content_size.width() - bounds.width() - clipping.x(), 0),
        std::min(content_size.height() - bounds.height() - clipping.y(), 0));
    content_layer->children()[0]->SetPosition(offset);
  }
}

void ContentLayer::ClipStaticLayer(scoped_refptr<ThumbnailLayer> static_layer,
                                   gfx::Rect clipping) {
  if (!static_layer.get())
    return;
  static_layer->Clip(clipping);
}

}  //  namespace android
}  //  namespace chrome
