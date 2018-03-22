// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/child_frame_compositing_helper.h"

#include <utility>

#include "build/build_config.h"
#include "cc/blink/web_layer_impl.h"
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/surface_layer.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "content/renderer/child_frame_compositor.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/skia_util.h"

namespace content {

ChildFrameCompositingHelper::ChildFrameCompositingHelper(
    ChildFrameCompositor* child_frame_compositor)
    : child_frame_compositor_(child_frame_compositor) {
  DCHECK(child_frame_compositor_);
}

ChildFrameCompositingHelper::~ChildFrameCompositingHelper() = default;

void ChildFrameCompositingHelper::ChildFrameGone(
    const gfx::Size& frame_size_in_dip,
    float device_scale_factor) {
  primary_surface_id_ = viz::SurfaceId();
  fallback_surface_id_ = viz::SurfaceId();

  scoped_refptr<cc::SolidColorLayer> crashed_layer =
      cc::SolidColorLayer::Create();
  crashed_layer->SetMasksToBounds(true);
  crashed_layer->SetBackgroundColor(SK_ColorBLACK);

  if (child_frame_compositor_->GetLayer()) {
    SkBitmap* sad_bitmap = child_frame_compositor_->GetSadPageBitmap();
    if (sad_bitmap && frame_size_in_dip.width() > sad_bitmap->width() &&
        frame_size_in_dip.height() > sad_bitmap->height()) {
      scoped_refptr<cc::PictureImageLayer> sad_layer =
          cc::PictureImageLayer::Create();
      sad_layer->SetImage(cc::PaintImageBuilder::WithDefault()
                              .set_id(cc::PaintImage::GetNextId())
                              .set_image(SkImage::MakeFromBitmap(*sad_bitmap),
                                         cc::PaintImage::GetNextContentId())
                              .TakePaintImage(),
                          SkMatrix::I(), false);
      sad_layer->SetBounds(
          gfx::Size(sad_bitmap->width() * device_scale_factor,
                    sad_bitmap->height() * device_scale_factor));
      sad_layer->SetPosition(
          gfx::PointF((frame_size_in_dip.width() - sad_bitmap->width()) / 2,
                      (frame_size_in_dip.height() - sad_bitmap->height()) / 2));
      sad_layer->SetIsDrawable(true);

      crashed_layer->AddChild(sad_layer);
    }
  }

  child_frame_compositor_->SetLayer(
      std::make_unique<cc_blink::WebLayerImpl>(crashed_layer));
}

void ChildFrameCompositingHelper::SetPrimarySurfaceId(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_dip) {
  if (primary_surface_id_ == surface_id)
    return;

  primary_surface_id_ = surface_id;

  surface_layer_ = cc::SurfaceLayer::Create();
  surface_layer_->SetMasksToBounds(true);
  surface_layer_->SetHitTestable(true);
  surface_layer_->SetBackgroundColor(SK_ColorTRANSPARENT);

  surface_layer_->SetPrimarySurfaceId(surface_id,
                                      cc::DeadlinePolicy::UseDefaultDeadline());
  surface_layer_->SetFallbackSurfaceId(fallback_surface_id_);

  std::unique_ptr<cc_blink::WebLayerImpl> layer(
      new cc_blink::WebLayerImpl(surface_layer_));
  // TODO(lfg): Investigate if it's possible to propagate the information about
  // the child surface's opacity. https://crbug.com/629851.
  layer->SetOpaque(false);
  layer->SetContentsOpaqueIsFixed(true);
  child_frame_compositor_->SetLayer(std::move(layer));

  UpdateVisibility(true);

  static_cast<cc_blink::WebLayerImpl*>(child_frame_compositor_->GetLayer())
      ->layer()
      ->SetBounds(frame_size_in_dip);
}

void ChildFrameCompositingHelper::SetFallbackSurfaceId(
    const viz::SurfaceId& surface_id,
    const gfx::Size& frame_size_in_dip) {
  if (fallback_surface_id_ == surface_id)
    return;

  fallback_surface_id_ = surface_id;

  if (!surface_layer_) {
    SetPrimarySurfaceId(surface_id, frame_size_in_dip);
    return;
  }

  surface_layer_->SetFallbackSurfaceId(surface_id);
}

void ChildFrameCompositingHelper::UpdateVisibility(bool visible) {
  blink::WebLayer* web_layer = child_frame_compositor_->GetLayer();
  if (web_layer)
    web_layer->SetDrawsContent(visible);
}

}  // namespace content
