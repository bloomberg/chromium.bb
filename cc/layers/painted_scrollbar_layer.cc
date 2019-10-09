// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/painted_scrollbar_layer.h"

#include "base/auto_reset.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_host.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

std::unique_ptr<LayerImpl> PaintedScrollbarLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PaintedScrollbarLayerImpl::Create(
      tree_impl, id(), scrollbar_->Orientation(),
      scrollbar_->IsLeftSideVerticalScrollbar(), scrollbar_->IsOverlay());
}

scoped_refptr<PaintedScrollbarLayer> PaintedScrollbarLayer::Create(
    std::unique_ptr<Scrollbar> scrollbar) {
  return base::WrapRefCounted(new PaintedScrollbarLayer(std::move(scrollbar)));
}

PaintedScrollbarLayer::PaintedScrollbarLayer(
    std::unique_ptr<Scrollbar> scrollbar)
    : scrollbar_(std::move(scrollbar)),
      internal_contents_scale_(1.f),
      supports_drag_snap_back_(false),
      thumb_thickness_(scrollbar_->ThumbThickness()),
      thumb_length_(scrollbar_->ThumbLength()),
      is_overlay_(scrollbar_->IsOverlay()),
      has_thumb_(scrollbar_->HasThumb()),
      thumb_opacity_(scrollbar_->ThumbOpacity()) {}

PaintedScrollbarLayer::~PaintedScrollbarLayer() = default;

bool PaintedScrollbarLayer::OpacityCanAnimateOnImplThread() const {
  return scrollbar_->IsOverlay();
}

void PaintedScrollbarLayer::PushPropertiesTo(LayerImpl* layer) {
  ScrollbarLayerBase::PushPropertiesTo(layer);

  PaintedScrollbarLayerImpl* scrollbar_layer =
      static_cast<PaintedScrollbarLayerImpl*>(layer);

  scrollbar_layer->set_internal_contents_scale_and_bounds(
      internal_contents_scale_, internal_content_bounds_);

  scrollbar_layer->SetSupportsDragSnapBack(supports_drag_snap_back_);
  scrollbar_layer->SetThumbThickness(thumb_thickness_);
  scrollbar_layer->SetBackButtonRect(back_button_rect_);
  scrollbar_layer->SetForwardButtonRect(forward_button_rect_);
  scrollbar_layer->SetThumbLength(thumb_length_);
  if (scrollbar_->Orientation() == HORIZONTAL) {
    scrollbar_layer->SetTrackStart(track_rect_.x());
    scrollbar_layer->SetTrackLength(track_rect_.width());
  } else {
    scrollbar_layer->SetTrackStart(track_rect_.y());
    scrollbar_layer->SetTrackLength(track_rect_.height());
  }

  if (track_resource_.get())
    scrollbar_layer->set_track_ui_resource_id(track_resource_->id());
  else
    scrollbar_layer->set_track_ui_resource_id(0);
  if (thumb_resource_.get())
    scrollbar_layer->set_thumb_ui_resource_id(thumb_resource_->id());
  else
    scrollbar_layer->set_thumb_ui_resource_id(0);

  scrollbar_layer->set_thumb_opacity(thumb_opacity_);

  scrollbar_layer->set_is_overlay_scrollbar(is_overlay_);
}

void PaintedScrollbarLayer::SetLayerTreeHost(LayerTreeHost* host) {
  // When the LTH is set to null or has changed, then this layer should remove
  // all of its associated resources.
  if (!host || host != layer_tree_host()) {
    track_resource_ = nullptr;
    thumb_resource_ = nullptr;
  }

  ScrollbarLayerBase::SetLayerTreeHost(host);
}

gfx::Rect PaintedScrollbarLayer::ScrollbarLayerRectToContentRect(
    const gfx::Rect& layer_rect) const {
  // Don't intersect with the bounds as in LayerRectToContentRect() because
  // layer_rect here might be in coordinates of the containing layer.
  gfx::Rect expanded_rect = gfx::ScaleToEnclosingRectSafe(
      layer_rect, internal_contents_scale_, internal_contents_scale_);
  // We should never return a rect bigger than the content bounds.
  gfx::Size clamped_size = expanded_rect.size();
  clamped_size.SetToMin(internal_content_bounds_);
  expanded_rect.set_size(clamped_size);
  return expanded_rect;
}

gfx::Rect PaintedScrollbarLayer::OriginThumbRect() const {
  gfx::Size thumb_size;
  if (scrollbar_->Orientation() == HORIZONTAL) {
    thumb_size =
        gfx::Size(scrollbar_->ThumbLength(), scrollbar_->ThumbThickness());
  } else {
    thumb_size =
        gfx::Size(scrollbar_->ThumbThickness(), scrollbar_->ThumbLength());
  }
  return gfx::Rect(thumb_size);
}

void PaintedScrollbarLayer::UpdateThumbAndTrackGeometry() {
  UpdateProperty(scrollbar_->SupportsDragSnapBack(), &supports_drag_snap_back_);
  UpdateProperty(scrollbar_->TrackRect(), &track_rect_);
  UpdateProperty(scrollbar_->BackButtonRect(), &back_button_rect_);
  UpdateProperty(scrollbar_->ForwardButtonRect(), &forward_button_rect_);
  UpdateProperty(scrollbar_->Location(), &location_);
  UpdateProperty(scrollbar_->IsOverlay(), &is_overlay_);
  UpdateProperty(scrollbar_->HasThumb(), &has_thumb_);
  if (has_thumb_) {
    UpdateProperty(scrollbar_->ThumbThickness(), &thumb_thickness_);
    UpdateProperty(scrollbar_->ThumbLength(), &thumb_length_);
  } else {
    UpdateProperty(0, &thumb_thickness_);
    UpdateProperty(0, &thumb_length_);
  }
}

void PaintedScrollbarLayer::UpdateInternalContentScale() {
  gfx::Transform transform;
  transform = draw_property_utils::ScreenSpaceTransform(
      this, layer_tree_host()->property_trees()->transform_tree);

  gfx::Vector2dF transform_scales = MathUtil::ComputeTransform2dScaleComponents(
      transform, layer_tree_host()->device_scale_factor());
  float scale = std::max(transform_scales.x(), transform_scales.y());

  bool changed = false;
  changed |= UpdateProperty(scale, &internal_contents_scale_);
  changed |=
      UpdateProperty(gfx::ScaleToCeiledSize(bounds(), internal_contents_scale_),
                     &internal_content_bounds_);
  if (changed) {
    // If the content scale or bounds change, repaint.
    SetNeedsDisplay();
  }
}

bool PaintedScrollbarLayer::Update() {
  {
    auto ignore_set_needs_commit = IgnoreSetNeedsCommit();
    ScrollbarLayerBase::Update();
    UpdateInternalContentScale();
  }

  UpdateThumbAndTrackGeometry();

  gfx::Rect track_layer_rect = gfx::Rect(location_, bounds());
  gfx::Rect scaled_track_rect = ScrollbarLayerRectToContentRect(
      track_layer_rect);

  bool updated = false;

  if (scaled_track_rect.IsEmpty()) {
    if (track_resource_) {
      track_resource_ = nullptr;
      thumb_resource_ = nullptr;
      SetNeedsPushProperties();
      updated = true;
    }
    return updated;
  }

  if (!has_thumb_ && thumb_resource_) {
    thumb_resource_ = nullptr;
    SetNeedsPushProperties();
    updated = true;
  }

  if (update_rect().IsEmpty() && track_resource_)
    return updated;

  if (!track_resource_ || scrollbar_->NeedsPaintPart(TRACK)) {
    track_resource_ = ScopedUIResource::Create(
        layer_tree_host()->GetUIResourceManager(),
        RasterizeScrollbarPart(track_layer_rect, scaled_track_rect, TRACK));
  }

  gfx::Rect thumb_layer_rect = OriginThumbRect();
  gfx::Rect scaled_thumb_rect =
      ScrollbarLayerRectToContentRect(thumb_layer_rect);
  if (has_thumb_ && !scaled_thumb_rect.IsEmpty()) {
    if (!thumb_resource_ || scrollbar_->NeedsPaintPart(THUMB) ||
        scaled_thumb_rect.size() !=
            thumb_resource_->GetBitmap(0, false).GetSize()) {
      thumb_resource_ = ScopedUIResource::Create(
          layer_tree_host()->GetUIResourceManager(),
          RasterizeScrollbarPart(thumb_layer_rect, scaled_thumb_rect, THUMB));
    }
    thumb_opacity_ = scrollbar_->ThumbOpacity();
  }

  // UI resources changed so push properties is needed.
  SetNeedsPushProperties();
  updated = true;
  return updated;
}

UIResourceBitmap PaintedScrollbarLayer::RasterizeScrollbarPart(
    const gfx::Rect& layer_rect,
    const gfx::Rect& requested_content_rect,
    ScrollbarPart part) {
  DCHECK(!requested_content_rect.size().IsEmpty());
  DCHECK(!layer_rect.size().IsEmpty());

  gfx::Rect content_rect = requested_content_rect;

  // Pages can end up requesting arbitrarily large scrollbars.  Prevent this
  // from crashing due to OOM and try something smaller.
  SkBitmap skbitmap;
  bool allocation_succeeded =
      skbitmap.tryAllocN32Pixels(content_rect.width(), content_rect.height());
  // Assuming 4bpp, caps at 4M.
  constexpr int kMinScrollbarDimension = 1024;
  int dimension = std::max(content_rect.width(), content_rect.height()) / 2;
  while (!allocation_succeeded && dimension >= kMinScrollbarDimension) {
    content_rect.Intersect(gfx::Rect(requested_content_rect.x(),
                                     requested_content_rect.y(), dimension,
                                     dimension));
    allocation_succeeded =
        skbitmap.tryAllocN32Pixels(content_rect.width(), content_rect.height());
    if (!allocation_succeeded)
      dimension = dimension / 2;
  }
  CHECK(allocation_succeeded)
      << "Failed to allocate memory for scrollbar at dimension : " << dimension;

  SkiaPaintCanvas canvas(skbitmap);
  canvas.clear(SK_ColorTRANSPARENT);

  float scale_x =
      content_rect.width() / static_cast<float>(layer_rect.width());
  float scale_y =
      content_rect.height() / static_cast<float>(layer_rect.height());
  canvas.scale(SkFloatToScalar(scale_x), SkFloatToScalar(scale_y));

  scrollbar_->PaintPart(&canvas, part);
  // Make sure that the pixels are no longer mutable to unavoid unnecessary
  // allocation and copying.
  skbitmap.setImmutable();

  return UIResourceBitmap(skbitmap);
}

}  // namespace cc
