// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>

#include "base/debug/trace_event.h"
#include "cc/base/region.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/debug_colors.h"
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

scoped_refptr<PicturePileImpl> PicturePileImpl::Create() {
  return make_scoped_refptr(new PicturePileImpl);
}

scoped_refptr<PicturePileImpl> PicturePileImpl::CreateFromOther(
    const PicturePileBase* other) {
  return make_scoped_refptr(new PicturePileImpl(other));
}

scoped_refptr<PicturePileImpl> PicturePileImpl::CreateCloneForDrawing(
    const PicturePileImpl* other, unsigned thread_index) {
  return make_scoped_refptr(new PicturePileImpl(other, thread_index));
}

PicturePileImpl::PicturePileImpl()
    : clones_for_drawing_(ClonesForDrawing(this, 0)) {
}

PicturePileImpl::PicturePileImpl(const PicturePileBase* other)
    : PicturePileBase(other),
      clones_for_drawing_(ClonesForDrawing(this, num_raster_threads())) {
}

PicturePileImpl::PicturePileImpl(
    const PicturePileImpl* other, unsigned thread_index)
    : PicturePileBase(other, thread_index),
      clones_for_drawing_(ClonesForDrawing(this, 0)) {
}

PicturePileImpl::~PicturePileImpl() {
}

PicturePileImpl* PicturePileImpl::GetCloneForDrawingOnThread(
    unsigned thread_index) const {
  CHECK_GT(clones_for_drawing_.clones_.size(), thread_index);
  return clones_for_drawing_.clones_[thread_index].get();
}

void PicturePileImpl::RasterDirect(
    SkCanvas* canvas,
    gfx::Rect canvas_rect,
    float contents_scale,
    RasterStats* raster_stats) {
  RasterCommon(canvas, NULL, canvas_rect, contents_scale, raster_stats);
}

void PicturePileImpl::RasterForAnalysis(
    skia::AnalysisCanvas* canvas,
    gfx::Rect canvas_rect,
    float contents_scale) {
  RasterCommon(canvas, canvas, canvas_rect, contents_scale, NULL);
}

void PicturePileImpl::RasterToBitmap(
    SkCanvas* canvas,
    gfx::Rect canvas_rect,
    float contents_scale,
    RasterStats* raster_stats) {
#ifndef NDEBUG
  // Any non-painted areas will be left in this color.
  canvas->clear(DebugColors::NonPaintedFillColor());
#endif  // NDEBUG

  // If this picture has opaque contents, it is guaranteeing that it will
  // draw an opaque rect the size of the layer.  If it is not, then we must
  // clear this canvas ourselves.
  if (!contents_opaque_) {
    // Clearing is about ~4x faster than drawing a rect even if the content
    // isn't covering a majority of the canvas.
    canvas->clear(SK_ColorTRANSPARENT);
  } else {
    // Even if it is opaque, on any rasterizations that touch the edge of the
    // layer, we also need to raster the background color underneath the last
    // texel (since the recording won't cover it) and outside the last texel
    // (due to linear filtering when using this texture).
    gfx::SizeF total_content_size = gfx::ScaleSize(tiling_.total_size(),
                                                   contents_scale);
    gfx::Rect content_rect(gfx::ToCeiledSize(total_content_size));
    gfx::Rect deflated_content_rect = content_rect;
    content_rect.Intersect(canvas_rect);

    // The final texel of content may only be partially covered by a
    // rasterization; this rect represents the content rect that is fully
    // covered by content.
    deflated_content_rect.Inset(0, 0, 1, 1);
    deflated_content_rect.Intersect(canvas_rect);
    if (!deflated_content_rect.Contains(canvas_rect)) {
      // Drawing at most 2 x 2 x (canvas width + canvas height) texels is 2-3X
      // faster than clearing, so special case this.
      canvas->save();
      gfx::Rect inflated_content_rect = content_rect;
      inflated_content_rect.Inset(0, 0, -1, -1);
      canvas->clipRect(gfx::RectToSkRect(inflated_content_rect),
                       SkRegion::kReplace_Op);
      canvas->clipRect(gfx::RectToSkRect(deflated_content_rect),
                       SkRegion::kDifference_Op);
      canvas->drawColor(background_color_, SkXfermode::kSrc_Mode);
      canvas->restore();
    }
  }

  RasterCommon(canvas, NULL, canvas_rect, contents_scale, raster_stats);
}

void PicturePileImpl::RasterCommon(
    SkCanvas* canvas,
    SkDrawPictureCallback* callback,
    gfx::Rect canvas_rect,
    float contents_scale,
    RasterStats* raster_stats) {
  DCHECK(contents_scale >= min_contents_scale_);

  canvas->translate(-canvas_rect.x(), -canvas_rect.y());

  gfx::SizeF total_content_size = gfx::ScaleSize(tiling_.total_size(),
                                                 contents_scale);
  gfx::Rect total_content_rect(gfx::ToCeiledSize(total_content_size));
  gfx::Rect content_rect = total_content_rect;
  content_rect.Intersect(canvas_rect);

  // Rasterize the collection of relevant picture piles.
  gfx::Rect layer_rect = gfx::ScaleToEnclosingRect(
      content_rect, 1.f / contents_scale);

  canvas->clipRect(gfx::RectToSkRect(content_rect),
                   SkRegion::kIntersect_Op);
  Region unclipped(content_rect);

  if (raster_stats) {
    raster_stats->total_pixels_rasterized = 0;
    raster_stats->total_rasterize_time = base::TimeDelta::FromSeconds(0);
    raster_stats->best_rasterize_time = base::TimeDelta::FromSeconds(0);
  }

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
      gfx::Rect content_clip = gfx::ScaleToEnclosedRect(
          (*i)->LayerRect(), contents_scale);
      DCHECK(!content_clip.IsEmpty()) <<
          "Layer rect: " << (*i)->LayerRect().ToString() <<
          "Contents scale: " << contents_scale;
      if (!unclipped.Intersects(content_clip))
        continue;

      base::TimeDelta total_duration =
          base::TimeDelta::FromInternalValue(0);
      base::TimeDelta best_duration =
          base::TimeDelta::FromInternalValue(std::numeric_limits<int64>::max());
      int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);

      TRACE_EVENT0(benchmark_instrumentation::kCategory,
                   benchmark_instrumentation::kRasterLoop);
      for (int j = 0; j < repeat_count; ++j) {
        base::TimeTicks start_time;
        if (raster_stats)
          start_time = base::TimeTicks::HighResNow();

        (*i)->Raster(canvas, callback, content_clip, contents_scale);

        if (raster_stats) {
          base::TimeDelta duration = base::TimeTicks::HighResNow() - start_time;
          total_duration += duration;
          best_duration = std::min(best_duration, duration);
        }
      }

      if (raster_stats) {
        gfx::Rect raster_rect = canvas_rect;
        raster_rect.Intersect(content_clip);
        raster_stats->total_pixels_rasterized +=
            repeat_count * raster_rect.width() * raster_rect.height();
        raster_stats->total_rasterize_time += total_duration;
        raster_stats->best_rasterize_time += best_duration;
      }

      if (show_debug_picture_borders_) {
        gfx::Rect border = content_clip;
        border.Inset(0, 0, 1, 1);

        SkPaint picture_border_paint;
        picture_border_paint.setColor(DebugColors::PictureBorderColor());
        canvas->drawLine(border.x(), border.y(), border.right(), border.y(),
                         picture_border_paint);
        canvas->drawLine(border.right(), border.y(), border.right(),
                         border.bottom(), picture_border_paint);
        canvas->drawLine(border.right(), border.bottom(), border.x(),
                         border.bottom(), picture_border_paint);
        canvas->drawLine(border.x(), border.bottom(), border.x(), border.y(),
                         picture_border_paint);
      }

      // Don't allow pictures underneath to draw where this picture did.
      canvas->clipRect(
          gfx::RectToSkRect(content_clip),
          SkRegion::kDifference_Op);
      unclipped.Subtract(content_clip);
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

  RasterToBitmap(canvas, layer_rect, 1.0, NULL);
  picture->endRecording();

  return picture;
}

void PicturePileImpl::AnalyzeInRect(gfx::Rect content_rect,
                                    float contents_scale,
                                    PicturePileImpl::Analysis* analysis) {
  DCHECK(analysis);
  TRACE_EVENT0("cc", "PicturePileImpl::AnalyzeInRect");

  gfx::Rect layer_rect = gfx::ScaleToEnclosingRect(
      content_rect, 1.0f / contents_scale);

  layer_rect.Intersect(gfx::Rect(tiling_.total_size()));

  SkBitmap empty_bitmap;
  empty_bitmap.setConfig(SkBitmap::kNo_Config,
                         layer_rect.width(),
                         layer_rect.height());
  skia::AnalysisDevice device(empty_bitmap);
  skia::AnalysisCanvas canvas(&device);

  RasterForAnalysis(&canvas, layer_rect, 1.0f);

  analysis->is_solid_color = canvas.GetColorIfSolid(&analysis->solid_color);
  analysis->has_text = canvas.HasText();
}

PicturePileImpl::Analysis::Analysis()
    : is_solid_color(false),
      has_text(false) {
}

PicturePileImpl::Analysis::~Analysis() {
}

PicturePileImpl::PixelRefIterator::PixelRefIterator(
    gfx::Rect content_rect,
    float contents_scale,
    const PicturePileImpl* picture_pile)
    : picture_pile_(picture_pile),
      layer_rect_(gfx::ScaleToEnclosingRect(
          content_rect, 1.f / contents_scale)),
      tile_iterator_(&picture_pile_->tiling_, layer_rect_),
      picture_list_(NULL) {
  // Early out if there isn't a single tile.
  if (!tile_iterator_)
    return;

  if (AdvanceToTileWithPictures())
    AdvanceToPictureWithPixelRefs();
}

PicturePileImpl::PixelRefIterator::~PixelRefIterator() {
}

PicturePileImpl::PixelRefIterator&
    PicturePileImpl::PixelRefIterator::operator++() {
  ++pixel_ref_iterator_;
  if (pixel_ref_iterator_)
    return *this;

  ++picture_list_iterator_;
  AdvanceToPictureWithPixelRefs();
  return *this;
}

bool PicturePileImpl::PixelRefIterator::AdvanceToTileWithPictures() {
  for (; tile_iterator_; ++tile_iterator_) {
    PictureListMap::const_iterator map_iterator =
        picture_pile_->picture_list_map_.find(tile_iterator_.index());
    if (map_iterator != picture_pile_->picture_list_map_.end()) {
      picture_list_ = &map_iterator->second;
      picture_list_iterator_ = picture_list_->begin();
      return true;
    }
  }

  return false;
}

void PicturePileImpl::PixelRefIterator::AdvanceToPictureWithPixelRefs() {
  DCHECK(tile_iterator_);
  do {
    for (;
         picture_list_iterator_ != picture_list_->end();
         ++picture_list_iterator_) {
      pixel_ref_iterator_ =
          Picture::PixelRefIterator(layer_rect_, picture_list_iterator_->get());
      if (pixel_ref_iterator_)
        return;
    }
    ++tile_iterator_;
  } while (AdvanceToTileWithPictures());
}

void PicturePileImpl::DidBeginTracing() {
  gfx::Rect layer_rect(tiling_.total_size());
  for (PictureListMap::iterator pli = picture_list_map_.begin();
       pli != picture_list_map_.end();
       pli++) {
    PictureList& picture_list = (*pli).second;
    for (PictureList::iterator picture = picture_list.begin();
         picture != picture_list.end();
         picture++) {
      (*picture)->EmitTraceSnapshot();
    }
  }
}

}  // namespace cc
