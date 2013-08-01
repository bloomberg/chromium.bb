// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer.h"

#include "base/auto_reset.h"
#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "skia/ext/platform_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSize.h"
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
  return make_scoped_refptr(
      new ScrollbarLayer(scrollbar.Pass(), scroll_layer_id));
}

ScrollbarLayer::ScrollbarLayer(
    scoped_ptr<Scrollbar> scrollbar,
    int scroll_layer_id)
    : scrollbar_(scrollbar.Pass()),
      scroll_layer_id_(scroll_layer_id) {
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

  if (track_resource_.get())
    scrollbar_layer->set_track_ui_resource_id(track_resource_->id());
  if (thumb_resource_.get())
    scrollbar_layer->set_thumb_ui_resource_id(thumb_resource_->id());

  scrollbar_layer->set_is_overlay_scrollbar(scrollbar_->IsOverlay());

  // ScrollbarLayer must push properties every frame. crbug.com/259095
  needs_push_properties_ = true;
}

ScrollbarLayer* ScrollbarLayer::ToScrollbarLayer() {
  return this;
}

void ScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  // When the LTH is set to null or has changed, then this layer should remove
  // all of its associated resources.
  if (!host || host != layer_tree_host()) {
    track_resource_.reset();
    thumb_resource_.reset();
  }

  ContentsScalingLayer::SetLayerTreeHost(host);
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

gfx::Rect ScrollbarLayer::OriginThumbRect() const {
  gfx::Size thumb_size;
  if (Orientation() == HORIZONTAL) {
    thumb_size =
        gfx::Size(scrollbar_->ThumbLength(), scrollbar_->ThumbThickness());
  } else {
    thumb_size =
        gfx::Size(scrollbar_->ThumbThickness(), scrollbar_->ThumbLength());
  }
  return ScrollbarLayerRectToContentRect(gfx::Rect(thumb_size));
}

bool ScrollbarLayer::Update(ResourceUpdateQueue* queue,
                            const OcclusionTracker* occlusion) {
  track_rect_ = scrollbar_->TrackRect();

  if (layer_tree_host()->settings().solid_color_scrollbars)
    return false;

  {
    base::AutoReset<bool> ignore_set_needs_commit(&ignore_set_needs_commit_,
                                                  true);
    ContentsScalingLayer::Update(queue, occlusion);
  }

  if (scrollbar_->HasThumb()) {
    thumb_thickness_ = scrollbar_->ThumbThickness();
    thumb_length_ = scrollbar_->ThumbLength();
  }

  track_resource_ =
    ScopedUIResource::Create(layer_tree_host(), RasterizeTrack());
  if (scrollbar_->HasThumb())
    thumb_resource_ =
      ScopedUIResource::Create(layer_tree_host(), RasterizeThumb());

  return true;
}

scoped_refptr<UIResourceBitmap> ScrollbarLayer::RasterizeTrack() {
  DCHECK(!layer_tree_host()->settings().solid_color_scrollbars);

  gfx::Rect track_rect = ScrollbarLayerRectToContentRect(
      gfx::Rect(scrollbar_->Location(), bounds()));
  scoped_refptr<UIResourceBitmap> track_bitmap = UIResourceBitmap::Create(
      new uint8_t[track_rect.width() * track_rect.height() * 4],
      UIResourceBitmap::RGBA8,
      track_rect.size());

  SkBitmap skbitmap;
  skbitmap.setConfig(
      SkBitmap::kARGB_8888_Config, track_rect.width(), track_rect.height());
  skbitmap.setPixels(track_bitmap->GetPixels());

  SkCanvas track_canvas(skbitmap);
  track_canvas.translate(SkFloatToScalar(-track_rect.x()),
                         SkFloatToScalar(-track_rect.y()));
  scrollbar_->PaintPart(&track_canvas, TRACK, track_rect);

  return track_bitmap;
}

scoped_refptr<UIResourceBitmap> ScrollbarLayer::RasterizeThumb() {
  DCHECK(!layer_tree_host()->settings().solid_color_scrollbars);
  DCHECK(scrollbar_->HasThumb());

  gfx::Rect thumb_rect = OriginThumbRect();
  scoped_refptr<UIResourceBitmap> thumb_bitmap = UIResourceBitmap::Create(
      new uint8_t[thumb_rect.width() * thumb_rect.height() * 4],
      UIResourceBitmap::RGBA8,
      thumb_rect.size());

  SkBitmap skbitmap;
  skbitmap.setConfig(
      SkBitmap::kARGB_8888_Config, thumb_rect.width(), thumb_rect.height());
  skbitmap.setPixels(thumb_bitmap->GetPixels());

  SkCanvas thumb_canvas(skbitmap);
  scrollbar_->PaintPart(&thumb_canvas, THUMB, thumb_rect);

  return thumb_bitmap;
}

}  // namespace cc
