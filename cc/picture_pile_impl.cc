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

PicturePileImpl::PicturePileImpl() {
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

  return clone;
}

void PicturePileImpl::Raster(
    SkCanvas* canvas,
    gfx::Rect content_rect,
    float contents_scale,
    RenderingStats* stats) {
  base::TimeTicks rasterizeBeginTime = base::TimeTicks::Now();

  // TODO(enne): do this more efficiently, i.e. top down with Skia clips
  canvas->save();
  canvas->translate(-content_rect.x(), -content_rect.y());
  SkRect layer_skrect = SkRect::MakeXYWH(
      content_rect.x(),
      content_rect.y(),
      content_rect.width(),
      content_rect.height());
  canvas->clipRect(layer_skrect);
  canvas->scale(contents_scale, contents_scale);

  gfx::Rect layer_rect = gfx::ToEnclosedRect(gfx::ScaleRect(gfx::RectF(content_rect), 1 / contents_scale));

  for (PicturePile::Pile::const_iterator i = pile_.begin();
       i != pile_.end(); ++i) {
    if (!(*i)->LayerRect().Intersects(layer_rect))
      continue;
    (*i)->Raster(canvas);

    SkISize deviceSize = canvas->getDeviceSize();
    stats->totalPixelsRasterized += deviceSize.width() * deviceSize.height();
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

}  // namespace cc
