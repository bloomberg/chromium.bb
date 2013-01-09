// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "cc/picture_pile_impl.h"
#include "cc/rendering_stats.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_refptr<PicturePileImpl> PicturePileImpl::Create() {
  return make_scoped_refptr(new PicturePileImpl());
}

PicturePileImpl::PicturePileImpl()
    : min_contents_scale_(1) {
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
  for (PicturePile::Pile::const_iterator i = pile_.begin();
       i != pile_.end(); ++i)
    clone->pile_.push_back((*i)->Clone());
  clone->min_contents_scale_ = min_contents_scale_;

  return clone;
}

void PicturePileImpl::Raster(
    SkCanvas* canvas,
    gfx::Rect content_rect,
    float contents_scale,
    RenderingStats* stats) {

  if (!pile_.size())
    return;

  DCHECK(contents_scale >= min_contents_scale_);

  base::TimeTicks rasterizeBeginTime = base::TimeTicks::Now();

  canvas->save();
  canvas->translate(-content_rect.x(), -content_rect.y());
  canvas->clipRect(gfx::RectToSkRect(content_rect));

  // Raster through the pile top down, using clips to make sure that
  // pictures on top are not overdrawn by pictures on the bottom.
  Region unclipped(content_rect);
  for (PicturePile::Pile::reverse_iterator i = pile_.rbegin();
       i != pile_.rend(); ++i) {
    // This is intentionally *enclosed* rect, so that the clip is aligned on
    // integral post-scale content pixels and does not extend past the edges of
    // the picture's layer rect.  The min_contents_scale enforces that enough
    // buffer pixels have been added such that the enclosed rect encompasses all
    // invalidated pixels at any larger scale level.
    gfx::Rect content_clip = gfx::ToEnclosedRect(
        gfx::ScaleRect((*i)->LayerRect(), contents_scale));
    if (!unclipped.Intersects(content_clip))
      continue;
    (*i)->Raster(canvas, content_clip, contents_scale);

    // Don't allow pictures underneath to draw where this picture did.
    canvas->clipRect(gfx::RectToSkRect(content_clip), SkRegion::kDifference_Op);
    unclipped.Subtract(content_clip);

    stats->totalPixelsRasterized +=
        content_clip.width() * content_clip.height();
  }
  canvas->restore();

  stats->totalRasterizeTimeInSeconds += (base::TimeTicks::Now() -
                                         rasterizeBeginTime).InSecondsF();
}

void PicturePileImpl::GatherPixelRefs(
    const gfx::Rect& rect, std::list<skia::LazyPixelRef*>& pixel_refs) {
  std::list<skia::LazyPixelRef*> result;
  for (PicturePile::Pile::const_iterator i = pile_.begin();
      i != pile_.end(); ++i) {
    (*i)->GatherPixelRefs(rect, result);
    pixel_refs.splice(pixel_refs.end(), result);
  }
}

skia::RefPtr<SkPicture> PicturePileImpl::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "PicturePileImpl::GetFlattenedPicture");

  gfx::Rect layer_rect;
  for (PicturePile::Pile::const_iterator i = pile_.begin();
      i != pile_.end(); ++i) {
    layer_rect.Union((*i)->LayerRect());
  }

  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  if (layer_rect.IsEmpty())
    return picture;

  SkCanvas* canvas = picture->beginRecording(
      layer_rect.width(),
      layer_rect.height(),
      SkPicture::kUsePathBoundsForClip_RecordingFlag);

  RenderingStats stats;
  Raster(canvas, layer_rect, 1.0, &stats);
  picture->endRecording();

  return picture;
}

}  // namespace cc
