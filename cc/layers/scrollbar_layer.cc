// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/scrollbar_layer.h"

#include "base/basictypes.h"
#include "base/debug/trace_event.h"
#include "cc/layers/scrollbar_layer_impl.h"
#include "cc/resources/caching_bitmap_content_layer_updater.h"
#include "cc/resources/layer_painter.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource_update_queue.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_ptr<LayerImpl> ScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return ScrollbarLayerImpl::Create(
      tree_impl,
      id(),
      ScrollbarGeometryFixedThumb::Create(make_scoped_ptr(geometry_->clone())))
      .PassAs<LayerImpl>();
}

scoped_refptr<ScrollbarLayer> ScrollbarLayer::Create(
    scoped_ptr<WebKit::WebScrollbar> scrollbar,
    scoped_ptr<ScrollbarThemePainter> painter,
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry,
    int scroll_layer_id) {
  return make_scoped_refptr(new ScrollbarLayer(scrollbar.Pass(),
                                               painter.Pass(),
                                               geometry.Pass(),
                                               scroll_layer_id));
}

ScrollbarLayer::ScrollbarLayer(
    scoped_ptr<WebKit::WebScrollbar> scrollbar,
    scoped_ptr<ScrollbarThemePainter> painter,
    scoped_ptr<WebKit::WebScrollbarThemeGeometry> geometry,
    int scroll_layer_id)
    : scrollbar_(scrollbar.Pass()),
      painter_(painter.Pass()),
      geometry_(geometry.Pass()),
      scroll_layer_id_(scroll_layer_id),
      texture_format_(GL_INVALID_ENUM) {
  if (!scrollbar_->isOverlay())
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
  return scrollbar_->isOverlay();
}

WebKit::WebScrollbar::Orientation ScrollbarLayer::Orientation() const {
  return scrollbar_->orientation();
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
                                            bool animating_transform_to_screen,
                                            float* contents_scale_x,
                                            float* contents_scale_y,
                                            gfx::Size* content_bounds) {
  ContentsScalingLayer::CalculateContentsScale(
      ClampScaleToMaxTextureSize(ideal_contents_scale),
      animating_transform_to_screen,
      contents_scale_x,
      contents_scale_y,
      content_bounds);
}

void ScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  ContentsScalingLayer::PushPropertiesTo(layer);

  ScrollbarLayerImpl* scrollbar_layer = static_cast<ScrollbarLayerImpl*>(layer);

  scrollbar_layer->SetScrollbarData(scrollbar_.get());
  scrollbar_layer->SetThumbSize(thumb_size_);

  if (back_track_ && back_track_->texture()->have_backing_texture()) {
    scrollbar_layer->set_back_track_resource_id(
        back_track_->texture()->resource_id());
  } else {
    scrollbar_layer->set_back_track_resource_id(0);
  }

  if (fore_track_ && fore_track_->texture()->have_backing_texture()) {
    scrollbar_layer->set_fore_track_resource_id(
        fore_track_->texture()->resource_id());
  } else {
    scrollbar_layer->set_fore_track_resource_id(0);
  }

  if (thumb_ && thumb_->texture()->have_backing_texture())
    scrollbar_layer->set_thumb_resource_id(thumb_->texture()->resource_id());
  else
    scrollbar_layer->set_thumb_resource_id(0);

  // Pinch zoom ScrollbarLayerImpl does not get its scroll_layer_id_
  // set in LayerImpl, so we need to push it here.
  if (scroll_layer_id_ == Layer::PINCH_ZOOM_ROOT_SCROLL_LAYER_ID)
    scrollbar_layer->set_scroll_layer_id(scroll_layer_id_);
}

ScrollbarLayer* ScrollbarLayer::ToScrollbarLayer() {
  return this;
}

class ScrollbarBackgroundPainter : public LayerPainter {
 public:
  static scoped_ptr<ScrollbarBackgroundPainter> Create(
      WebKit::WebScrollbar* scrollbar,
      ScrollbarThemePainter *painter,
      WebKit::WebScrollbarThemeGeometry* geometry,
      WebKit::WebScrollbar::ScrollbarPart trackPart) {
    return make_scoped_ptr(new ScrollbarBackgroundPainter(scrollbar,
                                                          painter,
                                                          geometry,
                                                          trackPart));
  }

  virtual void Paint(SkCanvas* canvas,
                     gfx::Rect content_rect,
                     gfx::RectF* opaque) OVERRIDE {
    // The following is a simplification of ScrollbarThemeComposite::paint.
    painter_->PaintScrollbarBackground(canvas, content_rect);

    if (geometry_->hasButtons(scrollbar_)) {
      gfx::Rect back_button_start_paint_rect =
          geometry_->backButtonStartRect(scrollbar_);
      painter_->PaintBackButtonStart(canvas, back_button_start_paint_rect);

      gfx::Rect back_button_end_paint_rect =
          geometry_->backButtonEndRect(scrollbar_);
      painter_->PaintBackButtonEnd(canvas, back_button_end_paint_rect);

      gfx::Rect forward_button_start_paint_rect =
          geometry_->forwardButtonStartRect(scrollbar_);
      painter_->PaintForwardButtonStart(canvas,
                                        forward_button_start_paint_rect);

      gfx::Rect forward_button_end_paint_rect =
          geometry_->forwardButtonEndRect(scrollbar_);
      painter_->PaintForwardButtonEnd(canvas, forward_button_end_paint_rect);
    }

    gfx::Rect track_paint_rect = geometry_->trackRect(scrollbar_);
    painter_->PaintTrackBackground(canvas, track_paint_rect);

    bool thumb_present = geometry_->hasThumb(scrollbar_);
    if (thumb_present) {
      if (track_part_ == WebKit::WebScrollbar::ForwardTrackPart)
        painter_->PaintForwardTrackPart(canvas, track_paint_rect);
      else
        painter_->PaintBackTrackPart(canvas, track_paint_rect);
    }

    painter_->PaintTickmarks(canvas, track_paint_rect);
  }
 private:
  ScrollbarBackgroundPainter(WebKit::WebScrollbar* scrollbar,
                             ScrollbarThemePainter *painter,
                             WebKit::WebScrollbarThemeGeometry* geometry,
                             WebKit::WebScrollbar::ScrollbarPart trackPart)
      : scrollbar_(scrollbar),
        painter_(painter),
        geometry_(geometry),
        track_part_(trackPart) {}

  WebKit::WebScrollbar* scrollbar_;
  ScrollbarThemePainter* painter_;
  WebKit::WebScrollbarThemeGeometry* geometry_;
  WebKit::WebScrollbar::ScrollbarPart track_part_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarBackgroundPainter);
};

class ScrollbarThumbPainter : public LayerPainter {
 public:
  static scoped_ptr<ScrollbarThumbPainter> Create(
      WebKit::WebScrollbar* scrollbar,
      ScrollbarThemePainter* painter,
      WebKit::WebScrollbarThemeGeometry* geometry) {
    return make_scoped_ptr(new ScrollbarThumbPainter(scrollbar,
                                                     painter,
                                                     geometry));
  }

  virtual void Paint(SkCanvas* canvas,
                     gfx::Rect content_rect,
                     gfx::RectF* opaque) OVERRIDE {
    // Consider the thumb to be at the origin when painting.
    gfx::Rect thumb_rect = geometry_->thumbRect(scrollbar_);
    painter_->PaintThumb(canvas, gfx::Rect(thumb_rect.size()));
  }

 private:
  ScrollbarThumbPainter(WebKit::WebScrollbar* scrollbar,
                        ScrollbarThemePainter* painter,
                        WebKit::WebScrollbarThemeGeometry* geometry)
      : scrollbar_(scrollbar),
        painter_(painter),
        geometry_(geometry) {}

  WebKit::WebScrollbar* scrollbar_;
  ScrollbarThemePainter* painter_;
  WebKit::WebScrollbarThemeGeometry* geometry_;

  DISALLOW_COPY_AND_ASSIGN(ScrollbarThumbPainter);
};

void ScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (!host || host != layer_tree_host()) {
    back_track_updater_ = NULL;
    back_track_.reset();
    thumb_updater_ = NULL;
    thumb_.reset();
  }

  ContentsScalingLayer::SetLayerTreeHost(host);
}

void ScrollbarLayer::CreateUpdaterIfNeeded() {
  if (layer_tree_host()->settings().solid_color_scrollbars)
    return;

  texture_format_ =
      layer_tree_host()->GetRendererCapabilities().best_texture_format;

  if (!back_track_updater_) {
    back_track_updater_ = CachingBitmapContentLayerUpdater::Create(
        ScrollbarBackgroundPainter::Create(
            scrollbar_.get(),
            painter_.get(),
            geometry_.get(),
            WebKit::WebScrollbar::BackTrackPart).PassAs<LayerPainter>());
  }
  if (!back_track_) {
    back_track_ = back_track_updater_->CreateResource(
        layer_tree_host()->contents_texture_manager());
  }

  // Only create two-part track if we think the two parts could be different in
  // appearance.
  if (scrollbar_->isCustomScrollbar()) {
    if (!fore_track_updater_) {
      fore_track_updater_ = CachingBitmapContentLayerUpdater::Create(
          ScrollbarBackgroundPainter::Create(
              scrollbar_.get(),
              painter_.get(),
              geometry_.get(),
              WebKit::WebScrollbar::ForwardTrackPart).PassAs<LayerPainter>());
    }
    if (!fore_track_) {
      fore_track_ = fore_track_updater_->CreateResource(
          layer_tree_host()->contents_texture_manager());
    }
  }

  if (!thumb_updater_) {
    thumb_updater_ = CachingBitmapContentLayerUpdater::Create(
        ScrollbarThumbPainter::Create(scrollbar_.get(),
                                      painter_.get(),
                                      geometry_.get()).PassAs<LayerPainter>());
  }
  if (!thumb_) {
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
  gfx::RectF content_rect = gfx::ScaleRect(layer_rect,
                                           contents_scale_y(),
                                           contents_scale_y());
  return gfx::ToEnclosingRect(content_rect);
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
  if (back_track_) {
    back_track_->texture()->SetDimensions(content_bounds(), texture_format_);
    back_track_->texture()->set_request_priority(
        PriorityCalculator::UIPriority(draws_to_root));
  }
  if (fore_track_) {
    fore_track_->texture()->SetDimensions(content_bounds(), texture_format_);
    fore_track_->texture()->set_request_priority(
        PriorityCalculator::UIPriority(draws_to_root));
  }
  if (thumb_) {
    gfx::Rect thumb_layer_rect = geometry_->thumbRect(scrollbar_.get());
    gfx::Size thumb_size =
        ScrollbarLayerRectToContentRect(thumb_layer_rect).size();
    thumb_->texture()->SetDimensions(thumb_size, texture_format_);
    thumb_->texture()->set_request_priority(
        PriorityCalculator::UIPriority(draws_to_root));
  }
}

void ScrollbarLayer::Update(ResourceUpdateQueue* queue,
                            const OcclusionTracker* occlusion,
                            RenderingStats* stats) {
  ContentsScalingLayer::Update(queue, occlusion, stats);

  dirty_rect_.Union(update_rect_);
  if (content_bounds().IsEmpty())
    return;
  if (visible_content_rect().IsEmpty())
    return;

  CreateUpdaterIfNeeded();

  gfx::Rect content_rect = ScrollbarLayerRectToContentRect(
      gfx::Rect(scrollbar_->location(), bounds()));
  UpdatePart(back_track_updater_.get(),
             back_track_.get(),
             content_rect,
             queue,
             stats);
  if (fore_track_ && fore_track_updater_) {
    UpdatePart(fore_track_updater_.get(),
               fore_track_.get(),
               content_rect,
               queue,
               stats);
  }

  // Consider the thumb to be at the origin when painting.
  gfx::Rect thumb_rect = geometry_->thumbRect(scrollbar_.get());
  thumb_size_ = thumb_rect.size();
  gfx::Rect origin_thumb_rect =
      ScrollbarLayerRectToContentRect(gfx::Rect(thumb_rect.size()));
  if (!origin_thumb_rect.IsEmpty()) {
    UpdatePart(thumb_updater_.get(),
               thumb_.get(),
               origin_thumb_rect,
               queue,
               stats);
  }

  dirty_rect_ = gfx::RectF();
}

}  // namespace cc
