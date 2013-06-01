
// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer.h"

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/resources/caching_bitmap_content_layer_updater.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/trees/layer_tree_host.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_ptr<LayerImpl> ScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return ScrollbarLayerImpl::Create(
      tree_impl, id(), scrollbar_->Orientation()).PassAs<LayerImpl>();
}

scoped_refptr<ScrollbarLayer> ScrollbarLayer::Create(
    scoped_ptr<Scrollbar> scrollbar,
    int scroll_layer_id) {
  return make_scoped_refptr(new ScrollbarLayer(scrollbar.Pass(),
                                               scroll_layer_id));
}

ScrollbarLayer::ScrollbarLayer(
    scoped_ptr<Scrollbar> scrollbar,
    int scroll_layer_id)
    : scrollbar_(scrollbar.Pass()),
      scroll_layer_id_(scroll_layer_id),
      texture_format_(GL_INVALID_ENUM) {
  if (!scrollbar_->IsOverlay())
    SetShouldScrollOnMainThread(true);
}

ScrollbarLayer::~ScrollbarLayer() {}

void ScrollbarLayer::SetScrollLayerId(int id) {
  if (id == scroll_layer_id_)
    return;

  scroll_layer_id_ = id;
  SetNeedsFullTreeSync();
}

bool ScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return scrollbar_->IsOverlay();
}

ScrollbarOrientation ScrollbarLayer::Orientation() const {
  return scrollbar_->Orientation();
}

int ScrollbarLayer::MaxTextureSize() {
  DCHECK(layer_tree_host());
  return layer_tree_host()->GetRendererCapabilities().max_texture_size;
}

float ScrollbarLayer::ClampScaleToMaxTextureSize(float scale) {
  if (layer_tree_host()->settings().solid_color_scrollbars)
    return scale;

  // If the scaled content_bounds() is bigger than the max texture size of the
  // device, we need to clamp it by rescaling, since content_bounds() is used
  // below to set the texture size.
  gfx::Size scaled_bounds = ComputeContentBoundsForScale(scale, scale);
  if (scaled_bounds.width() > MaxTextureSize() ||
      scaled_bounds.height() > MaxTextureSize()) {
    if (scaled_bounds.width() > scaled_bounds.height())
      return (MaxTextureSize() - 1) / static_cast<float>(bounds().width());
    else
      return (MaxTextureSize() - 1) / static_cast<float>(bounds().height());
  }
  return scale;
}

void ScrollbarLayer::CalculateContentsScale(float ideal_contents_scale,
                                            float device_scale_factor,
                                            float page_scale_factor,
                                            bool animating_transform_to_screen,
                                            float* contents_scale_x,
                                            float* contents_scale_y,
                                            gfx::Size* content_bounds) {
  ContentsScalingLayer::CalculateContentsScale(
      ClampScaleToMaxTextureSize(ideal_contents_scale),
      device_scale_factor,
      page_scale_factor,
      animating_transform_to_screen,
      contents_scale_x,
      contents_scale_y,
      content_bounds);
}

void ScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  ContentsScalingLayer::PushPropertiesTo(layer);

  ScrollbarLayerImpl* scrollbar_layer = static_cast<ScrollbarLayerImpl*>(layer);

  if (layer_tree_host() &&
      layer_tree_host()->settings().solid_color_scrollbars) {
    int thickness_override =
        layer_tree_host()->settings().solid_color_scrollbar_thickness_dip;
    if (thickness_override != -1) {
      scrollbar_layer->set_thumb_thickness(thickness_override);
    } else {
      if (Orientation() == HORIZONTAL)
        scrollbar_layer->set_thumb_thickness(bounds().height());
      else
        scrollbar_layer->set_thumb_thickness(bounds().width());
    }
  } else {
    scrollbar_layer->set_thumb_thickness(thumb_thickness_);
  }
  scrollbar_layer->set_thumb_length(thumb_length_);
  if (Orientation() == HORIZONTAL) {
    scrollbar_layer->set_track_start(track_rect_.x());
    scrollbar_layer->set_track_length(track_rect_.width());
  } else {
    scrollbar_layer->set_track_start(track_rect_.y());
    scrollbar_layer->set_track_length(track_rect_.height());
  }

  if (track_ && track_->texture()->have_backing_texture())
    scrollbar_layer->set_track_resource_id(track_->texture()->resource_id());
  else
    scrollbar_layer->set_track_resource_id(0);

  if (thumb_ && thumb_->texture()->have_backing_texture())
    scrollbar_layer->set_thumb_resource_id(thumb_->texture()->resource_id());
  else
    scrollbar_layer->set_thumb_resource_id(0);
}

ScrollbarLayer* ScrollbarLayer::ToScrollbarLayer() {
  return this;
}

void ScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (!host || host != layer_tree_host()) {
    track_updater_ = NULL;
    track_.reset();
    thumb_updater_ = NULL;
    thumb_.reset();
  }

  ContentsScalingLayer::SetLayerTreeHost(host);
}

class ScrollbarPartPainter : public LayerPainter {
 public:
  ScrollbarPartPainter(Scrollbar* scrollbar, ScrollbarPart part)
      : scrollbar_(scrollbar),
        part_(part) {}
  virtual ~ScrollbarPartPainter() {}

  // LayerPainter implementation
  virtual void Paint(SkCanvas* canvas,
                     gfx::Rect content_rect,
                     gfx::RectF* opaque) OVERRIDE {
    scrollbar_->PaintPart(canvas, part_, content_rect);
  }

 private:
  Scrollbar* scrollbar_;
  ScrollbarPart part_;
};

void ScrollbarLayer::CreateUpdaterIfNeeded() {
  if (layer_tree_host()->settings().solid_color_scrollbars)
    return;

  texture_format_ =
      layer_tree_host()->GetRendererCapabilities().best_texture_format;

  if (!track_updater_.get()) {
    track_updater_ = CachingBitmapContentLayerUpdater::Create(
        scoped_ptr<LayerPainter>(
            new ScrollbarPartPainter(scrollbar_.get(), TRACK))
            .Pass(),
        rendering_stats_instrumentation(),
        id());
  }
  if (!track_) {
    track_ = track_updater_->CreateResource(
        layer_tree_host()->contents_texture_manager());
  }

  if (!thumb_updater_.get()) {
    thumb_updater_ = CachingBitmapContentLayerUpdater::Create(
        scoped_ptr<LayerPainter>(
            new ScrollbarPartPainter(scrollbar_.get(), THUMB))
            .Pass(),
        rendering_stats_instrumentation(),
        id());
  }
  if (!thumb_ && scrollbar_->HasThumb()) {
    thumb_ = thumb_updater_->CreateResource(
        layer_tree_host()->contents_texture_manager());
  }
}

void ScrollbarLayer::UpdatePart(CachingBitmapContentLayerUpdater* painter,
                                LayerUpdater::Resource* resource,
                                gfx::Rect rect,
                                ResourceUpdateQueue* queue,
                                RenderingStats* stats) {
  if (layer_tree_host()->settings().solid_color_scrollbars)
    return;

  // Skip painting and uploading if there are no invalidations and
  // we already have valid texture data.
  if (resource->texture()->have_backing_texture() &&
      resource->texture()->size() == rect.size() &&
      !is_dirty())
    return;

  // We should always have enough memory for UI.
  DCHECK(resource->texture()->can_acquire_backing_texture());
  if (!resource->texture()->can_acquire_backing_texture())
    return;

  // Paint and upload the entire part.
  gfx::Rect painted_opaque_rect;
  painter->PrepareToUpdate(rect,
                           rect.size(),
                           contents_scale_x(),
                           contents_scale_y(),
                           &painted_opaque_rect,
                           stats);
  if (!painter->pixels_did_change() &&
      resource->texture()->have_backing_texture()) {
    TRACE_EVENT_INSTANT0("cc",
                         "ScrollbarLayer::UpdatePart no texture upload needed",
                         TRACE_EVENT_SCOPE_THREAD);
    return;
  }

  bool partial_updates_allowed =
      layer_tree_host()->settings().max_partial_texture_updates > 0;
  if (!partial_updates_allowed)
    resource->texture()->ReturnBackingTexture();

  gfx::Vector2d dest_offset(0, 0);
  resource->Update(queue, rect, dest_offset, partial_updates_allowed, stats);
}

gfx::Rect ScrollbarLayer::ScrollbarLayerRectToContentRect(
    gfx::Rect layer_rect) const {
  // Don't intersect with the bounds as in LayerRectToContentRect() because
  // layer_rect here might be in coordinates of the containing layer.
  gfx::Rect expanded_rect = gfx::ScaleToEnclosingRect(
      layer_rect, contents_scale_y(), contents_scale_y());
  // We should never return a rect bigger than the content_bounds().
  gfx::Size clamped_size = expanded_rect.size();
  clamped_size.SetToMin(content_bounds());
  expanded_rect.set_size(clamped_size);
  return expanded_rect;
}

void ScrollbarLayer::SetTexturePriorities(
    const PriorityCalculator& priority_calc) {
  if (layer_tree_host()->settings().solid_color_scrollbars)
    return;

  if (content_bounds().IsEmpty())
    return;
  DCHECK_LE(content_bounds().width(), MaxTextureSize());
  DCHECK_LE(content_bounds().height(), MaxTextureSize());

  CreateUpdaterIfNeeded();

  bool draws_to_root = !render_target()->parent();
  if (track_) {
    track_->texture()->SetDimensions(content_bounds(), texture_format_);
    track_->texture()->set_request_priority(
        PriorityCalculator::UIPriority(draws_to_root));
  }
  if (thumb_) {
    gfx::Size thumb_size = OriginThumbRect().size();
    thumb_->texture()->SetDimensions(thumb_size, texture_format_);
    thumb_->texture()->set_request_priority(
        PriorityCalculator::UIPriority(draws_to_root));
  }
}

void ScrollbarLayer::Update(ResourceUpdateQueue* queue,
                            const OcclusionTracker* occlusion,
                            RenderingStats* stats) {
  track_rect_ = scrollbar_->TrackRect();

  if (layer_tree_host()->settings().solid_color_scrollbars)
    return;

  {
    base::AutoReset<bool> ignore_set_needs_commit(&ignore_set_needs_commit_,
                                                  true);
    ContentsScalingLayer::Update(queue, occlusion, stats);
  }

  dirty_rect_.Union(update_rect_);
  if (content_bounds().IsEmpty())
    return;
  if (visible_content_rect().IsEmpty())
    return;

  CreateUpdaterIfNeeded();

  gfx::Rect content_rect = ScrollbarLayerRectToContentRect(
      gfx::Rect(scrollbar_->Location(), bounds()));
  UpdatePart(track_updater_.get(),
             track_.get(),
             content_rect,
             queue,
             stats);

  if (scrollbar_->HasThumb()) {
    thumb_thickness_ = scrollbar_->ThumbThickness();
    thumb_length_ = scrollbar_->ThumbLength();
    gfx::Rect origin_thumb_rect = OriginThumbRect();
    if (!origin_thumb_rect.IsEmpty()) {
      UpdatePart(thumb_updater_.get(),
                 thumb_.get(),
                 origin_thumb_rect,
                 queue,
                 stats);
    }
  }

  dirty_rect_ = gfx::RectF();
}

gfx::Rect ScrollbarLayer::OriginThumbRect() const {
  gfx::Size thumb_size;
  if (Orientation() == HORIZONTAL) {
    thumb_size = gfx::Size(scrollbar_->ThumbLength(),
                           scrollbar_->ThumbThickness());
  } else {
    thumb_size = gfx::Size(scrollbar_->ThumbThickness(),
                           scrollbar_->ThumbLength());
  }
  return ScrollbarLayerRectToContentRect(gfx::Rect(thumb_size));
}

}  // namespace cc
