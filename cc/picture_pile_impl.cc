// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "cc/debug_colors.h"
#include "cc/picture_pile_impl.h"
#include "cc/region.h"
#include "cc/rendering_stats.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

scoped_refptr<PicturePileImpl> PicturePileImpl::Create() {
  return make_scoped_refptr(new PicturePileImpl());
}

PicturePileImpl::PicturePileImpl()
    : slow_down_raster_scale_factor_for_debug_(0) {
}

PicturePileImpl::~PicturePileImpl() {
}

PicturePileImpl* PicturePileImpl::GetCloneForDrawingOnThread(
    base::Thread* thread) {
  // Do we have a clone for this thread yet?
  CloneMap::iterator it = clones_.find(thread->thread_id());
  if (it != clones_.end())
    return it->second;

  // Create clone for this thread.
  scoped_refptr<PicturePileImpl> clone = CloneForDrawing();
  clones_[thread->thread_id()] = clone;
  return clone;
}

scoped_refptr<PicturePileImpl> PicturePileImpl::CloneForDrawing() const {
  TRACE_EVENT0("cc", "PicturePileImpl::CloneForDrawing");
  scoped_refptr<PicturePileImpl> clone = Create();
  clone->tiling_ = tiling_;
  for (PictureListMap::const_iterator map_iter = picture_list_map_.begin();
       map_iter != picture_list_map_.end(); ++map_iter) {
    const PictureList& this_pic_list = map_iter->second;
    PictureList& clone_pic_list = clone->picture_list_map_[map_iter->first];
    for (PictureList::const_iterator pic_iter = this_pic_list.begin();
         pic_iter != this_pic_list.end(); ++pic_iter) {
      clone_pic_list.push_back((*pic_iter)->Clone());
    }
  }
  clone->min_contents_scale_ = min_contents_scale_;
  clone->set_slow_down_raster_scale_factor(
      slow_down_raster_scale_factor_for_debug_);

  return clone;
}

void PicturePileImpl::Raster(
    SkCanvas* canvas,
    gfx::Rect content_rect,
    float contents_scale,
    int64* total_pixels_rasterized) {

  DCHECK(contents_scale >= min_contents_scale_);

#ifndef NDEBUG
  // Any non-painted areas will be left in this color.
  canvas->clear(DebugColors::NonPaintedFillColor());
#endif  // NDEBUG

  canvas->save();
  canvas->translate(-content_rect.x(), -content_rect.y());
  canvas->clipRect(gfx::RectToSkRect(content_rect));

  gfx::Rect layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(content_rect, 1.f / contents_scale));

  Region unclipped(content_rect);
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
          (*i)->Raster(canvas, content_clip, contents_scale);
      } else {
        (*i)->Raster(canvas, content_clip, contents_scale);
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

void PicturePileImpl::PushPropertiesTo(PicturePileImpl* other) {
  PicturePileBase::PushPropertiesTo(other);
  other->clones_ = clones_;
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

  int64 total_pixels_rasterized = 0;
  Raster(canvas, layer_rect, 1.0, &total_pixels_rasterized);
  picture->endRecording();

  return picture;
}

bool PicturePileImpl::IsCheapInRect(
    gfx::Rect content_rect, float contents_scale) const {
  gfx::Rect layer_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(content_rect, 1.f / contents_scale));

  for (TilingData::Iterator tile_iter(&tiling_, layer_rect);
       tile_iter; ++tile_iter) {
    PictureListMap::const_iterator map_iter =
        picture_list_map_.find(tile_iter.index());
    if (map_iter == picture_list_map_.end())
      continue;

    const PictureList& pic_list = map_iter->second;
    for (PictureList::const_iterator i = pic_list.begin();
         i != pic_list.end(); ++i) {
      if (!(*i)->LayerRect().Intersects(layer_rect) || !(*i)->HasRecording())
        continue;
      if (!(*i)->IsCheapInRect(layer_rect))
        return false;
    }
  }
  return true;
}

}  // namespace cc
