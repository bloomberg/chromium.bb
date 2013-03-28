// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/debug/rendering_stats.h"
#include "cc/resources/picture_pile_impl.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

PicturePileImpl::ClonesForDrawing::ClonesForDrawing(
    const PicturePileImpl* pile, int num_threads) {
  for (int i = 0; i < num_threads; i++) {
    scoped_refptr<PicturePileImpl> clone =
        PicturePileImpl::CreateCloneForDrawing(pile, i);
    clones_.push_back(clone);
  }
}

PicturePileImpl::ClonesForDrawing::~ClonesForDrawing() {
}

scoped_refptr<PicturePileImpl> PicturePileImpl::Create(bool enable_lcd_text) {
  return make_scoped_refptr(new PicturePileImpl(enable_lcd_text));
}

scoped_refptr<PicturePileImpl> PicturePileImpl::CreateFromOther(
    const PicturePileBase* other,
    bool enable_lcd_text) {
  return make_scoped_refptr(new PicturePileImpl(other, enable_lcd_text));
}

scoped_refptr<PicturePileImpl> PicturePileImpl::CreateCloneForDrawing(
    const PicturePileImpl* other, unsigned thread_index) {
  return make_scoped_refptr(new PicturePileImpl(other, thread_index));
}

PicturePileImpl::PicturePileImpl(bool enable_lcd_text)
    : enable_lcd_text_(enable_lcd_text),
      clones_for_drawing_(ClonesForDrawing(this, 0)) {
}

PicturePileImpl::PicturePileImpl(const PicturePileBase* other,
                                 bool enable_lcd_text)
    : PicturePileBase(other),
      enable_lcd_text_(enable_lcd_text),
      clones_for_drawing_(ClonesForDrawing(this, num_raster_threads())) {
}

PicturePileImpl::PicturePileImpl(
    const PicturePileImpl* other, unsigned thread_index)
    : PicturePileBase(other, thread_index),
      enable_lcd_text_(other->enable_lcd_text_),
      clones_for_drawing_(ClonesForDrawing(this, 0)) {
}

PicturePileImpl::~PicturePileImpl() {
}

PicturePileImpl* PicturePileImpl::GetCloneForDrawingOnThread(
    unsigned thread_index) const {
  CHECK_GT(clones_for_drawing_.clones_.size(), thread_index);
  return clones_for_drawing_.clones_[thread_index];
}

int64 PicturePileImpl::Raster(
    SkCanvas* canvas,
    gfx::Rect canvas_rect,
    float contents_scale) {

  DCHECK(contents_scale >= min_contents_scale_);

#ifndef NDEBUG
  // Any non-painted areas will be left in this color.
  canvas->clear(DebugColors::NonPaintedFillColor());
#endif  // NDEBUG

  canvas->save();
  canvas->translate(-canvas_rect.x(), -canvas_rect.y());

  gfx::SizeF total_content_size = gfx::ScaleSize(tiling_.total_size(),
                                                 contents_scale);
  gfx::Rect total_content_rect(gfx::ToCeiledSize(total_content_size));
  gfx::Rect content_rect = total_content_rect;
  content_rect.Intersect(canvas_rect);

  // Clear one texel inside the right/bottom edge of the content rect,
  // as it may only be partially covered by the picture playback.
  // Also clear one texel outside the right/bottom edge of the content rect,
  // as it may get blended in by linear filtering when zoomed in.
  gfx::Rect deflated_content_rect = total_content_rect;
  deflated_content_rect.Inset(0, 0, 1, 1);

  gfx::Rect canvas_outside_content_rect = canvas_rect;
  canvas_outside_content_rect.Subtract(deflated_content_rect);

  if (!canvas_outside_content_rect.IsEmpty()) {
    gfx::Rect inflated_content_rect = total_content_rect;
    inflated_content_rect.Inset(0, 0, -1, -1);
    canvas->clipRect(gfx::RectToSkRect(inflated_content_rect),
                     SkRegion::kReplace_Op);
    canvas->clipRect(gfx::RectToSkRect(deflated_content_rect),
                     SkRegion::kDifference_Op);
    canvas->drawColor(background_color_, SkXfermode::kSrc_Mode);
  }

  // Rasterize the collection of relevant picture piles.
  gfx::Rect layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(content_rect, 1.f / contents_scale));

  canvas->clipRect(gfx::RectToSkRect(content_rect),
                   SkRegion::kReplace_Op);
  Region unclipped(content_rect);

  int64 total_pixels_rasterized = 0;
  for (TilingData::Iterator tile_iter(&tiling_, layer_rect);
       tile_iter; ++tile_iter) {
    PictureListMap::iterator map_iter =
        picture_list_map_.find(tile_iter.index());
    if (map_iter == picture_list_map_.end())
      continue;
    PictureList& pic_list= map_iter->second;
    if (pic_list.empty())
      continue;

    // Raster through the picture list top down, using clips to make sure that
    // pictures on top are not overdrawn by pictures on the bottom.
    for (PictureList::reverse_iterator i = pic_list.rbegin();
         i != pic_list.rend(); ++i) {
      // This is intentionally *enclosed* rect, so that the clip is aligned on
      // integral post-scale content pixels and does not extend past the edges
      // of the picture's layer rect.  The min_contents_scale enforces that
      // enough buffer pixels have been added such that the enclosed rect
      // encompasses all invalidated pixels at any larger scale level.
      gfx::Rect content_clip = gfx::ToEnclosedRect(
          gfx::ScaleRect((*i)->LayerRect(), contents_scale));
      DCHECK(!content_clip.IsEmpty());
      if (!unclipped.Intersects(content_clip))
        continue;

      if (slow_down_raster_scale_factor_for_debug_) {
        for (int j = 0; j < slow_down_raster_scale_factor_for_debug_; ++j)
          (*i)->Raster(canvas, content_clip, contents_scale, enable_lcd_text_);
      } else {
        (*i)->Raster(canvas, content_clip, contents_scale, enable_lcd_text_);
      }

      // Don't allow pictures underneath to draw where this picture did.
      canvas->clipRect(
          gfx::RectToSkRect(content_clip),
          SkRegion::kDifference_Op);
      unclipped.Subtract(content_clip);

      total_pixels_rasterized +=
          content_clip.width() * content_clip.height();
    }
  }

#ifndef NDEBUG
  // Fill the remaining clip with debug color. This allows us to
  // distinguish between non painted areas and problems with missing
  // pictures.
  SkPaint paint;
  paint.setColor(DebugColors::MissingPictureFillColor());
  paint.setXfermodeMode(SkXfermode::kSrc_Mode);
  canvas->drawPaint(paint);
#endif  // NDEBUG

  // We should always paint some part of |content_rect|.
  DCHECK(!unclipped.Contains(content_rect));

  canvas->restore();

  return total_pixels_rasterized;
}

void PicturePileImpl::GatherPixelRefs(
    gfx::Rect content_rect,
    float contents_scale,
    std::list<skia::LazyPixelRef*>& pixel_refs) {
  std::list<skia::LazyPixelRef*> result;

  gfx::Rect layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(content_rect, 1.f / contents_scale));

  for (TilingData::Iterator tile_iter(&tiling_, layer_rect);
       tile_iter; ++tile_iter) {
    PictureListMap::iterator map_iter =
        picture_list_map_.find(tile_iter.index());
    if (map_iter == picture_list_map_.end())
      continue;

    PictureList& pic_list = map_iter->second;
    for (PictureList::const_iterator i = pic_list.begin();
         i != pic_list.end(); ++i) {
      (*i)->GatherPixelRefs(layer_rect, result);
      pixel_refs.splice(pixel_refs.end(), result);
    }
  }
}

skia::RefPtr<SkPicture> PicturePileImpl::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "PicturePileImpl::GetFlattenedPicture");

  gfx::Rect layer_rect(tiling_.total_size());
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  if (layer_rect.IsEmpty())
    return picture;

  SkCanvas* canvas = picture->beginRecording(
      layer_rect.width(),
      layer_rect.height(),
      SkPicture::kUsePathBoundsForClip_RecordingFlag);

  Raster(canvas, layer_rect, 1.0);
  picture->endRecording();

  return picture;
}

void PicturePileImpl::AnalyzeInRect(const gfx::Rect& content_rect,
                                    float contents_scale,
                                    PicturePileImpl::Analysis* analysis) {
  DCHECK(analysis);
  TRACE_EVENT0("cc", "PicturePileImpl::AnalyzeInRect");

  gfx::Rect layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(content_rect, 1.f / contents_scale));

  SkBitmap empty_bitmap;
  empty_bitmap.setConfig(SkBitmap::kNo_Config, content_rect.width(),
                        content_rect.height());
  skia::AnalysisDevice device(empty_bitmap);
  skia::AnalysisCanvas canvas(&device);

  Raster(&canvas, content_rect, contents_scale);

  analysis->is_transparent = canvas.isTransparent();
  analysis->is_solid_color = canvas.getColorIfSolid(&analysis->solid_color);
  analysis->is_cheap_to_raster = canvas.isCheap();
  canvas.consumeLazyPixelRefs(&analysis->lazy_pixel_refs);
}

PicturePileImpl::Analysis::Analysis()
  : is_solid_color(false),
    is_transparent(false),
    is_cheap_to_raster(false) {
}

PicturePileImpl::Analysis::~Analysis() {
}

}  // namespace cc
