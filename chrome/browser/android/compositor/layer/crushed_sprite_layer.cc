// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/layer/crushed_sprite_layer.h"

#include "cc/layers/layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "content/public/browser/android/compositor.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/android/resources/crushed_sprite_resource.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"

namespace chrome {
namespace android {

// static
scoped_refptr<CrushedSpriteLayer> CrushedSpriteLayer::Create() {
  return make_scoped_refptr(new CrushedSpriteLayer());
}

scoped_refptr<cc::Layer> CrushedSpriteLayer::layer() {
  return layer_;
}

void CrushedSpriteLayer::DrawSpriteFrame(
    ui::ResourceManager* resource_manager,
    int bitmap_res_id,
    int metadata_res_id,
    float completion_percentage) {
  DCHECK(completion_percentage >= 0.f && completion_percentage <= 1.f);

  int sprite_frame = completion_percentage * (frame_count_ - 1);

  if (frame_count_ == -1 || sprite_frame != previous_frame_) {
    // Get resource and setup variables.
    ui::CrushedSpriteResource* resource =
        resource_manager->GetCrushedSpriteResource(
            bitmap_res_id,
            metadata_res_id);
    if (frame_count_ == -1) {
      frame_count_ = resource->GetFrameCount();
      sprite_frame = completion_percentage * (frame_count_ - 1);
    }

    // Reset the previous_frame if the animation is being re-run.
    if (previous_frame_ > sprite_frame) {
      previous_frame_ = -1;
    }

    // Set up an SkCanvas backed by an SkBitmap to draw into.
    SkBitmap bitmap;
    bitmap.allocN32Pixels(resource->GetUnscaledSpriteSize().width(),
                          resource->GetUnscaledSpriteSize().height());
    sk_sp<SkCanvas> canvas = sk_make_sp<SkCanvas>(bitmap);

    if (previous_frame_ == -1 ||
        sprite_frame == resource->GetFrameCount() - 1) {
      // The newly allocated pixels for the SkBitmap need to be cleared if this
      // is the first frame being drawn or the last frame. See crbug.com/549453.
      canvas->clear(SK_ColorTRANSPARENT);
    }

    // If this isn't the first or last frame, draw the previous frame(s).
    // Note(twellington): This assumes that the last frame in the crushed sprite
    // animation does not require any previous frames drawn before it. This code
    // needs to be updated if crushed sprites are added for which this
    // assumption does not hold.
    if (sprite_frame != 0 && sprite_frame != resource->GetFrameCount() - 1) {
      // Draw the previous frame.
      if (previous_frame_ != -1){
        canvas->drawBitmap(previous_frame_bitmap_, 0, 0, nullptr);
      }

      // Draw any skipped frames.
      for (int i = previous_frame_ + 1; i < sprite_frame; ++i) {
        DrawRectanglesForFrame(resource, i, canvas);
      }
    }

    // Draw the current frame.
    DrawRectanglesForFrame(resource, sprite_frame, canvas);

    // Set the bitmap on layer_.
    bitmap.setImmutable();
    layer_->SetBitmap(bitmap);

    // Set bounds to scale the layer.
    layer_->SetBounds(resource->GetScaledSpriteSize());

    // Evict the crushed sprite bitmap from memory if this is the last frame.
    if (sprite_frame == frame_count_ - 1) {
      resource->EvictBitmapFromMemory();
    }

    // Update previous_frame_* variables.
    previous_frame_bitmap_ = bitmap;
    previous_frame_ = sprite_frame;
  }
}

void CrushedSpriteLayer::DrawRectanglesForFrame(
    ui::CrushedSpriteResource* resource,
    int frame,
    sk_sp<SkCanvas> canvas) {
  ui::CrushedSpriteResource::FrameSrcDstRects src_dst_rects =
       resource->GetRectanglesForFrame(frame);
  for (const auto& rect : src_dst_rects) {
    canvas->drawBitmapRect(resource->GetBitmap(),
                           gfx::RectToSkRect(rect.first),
                           gfx::RectToSkRect(rect.second),
                           nullptr);
  }
}

CrushedSpriteLayer::CrushedSpriteLayer()
    : layer_(cc::UIResourceLayer::Create()),
      frame_count_(-1),
      previous_frame_(-1) {
  layer_->SetIsDrawable(true);
}


CrushedSpriteLayer::~CrushedSpriteLayer() {
}

}  // namespace android
}  // namespace chrome
