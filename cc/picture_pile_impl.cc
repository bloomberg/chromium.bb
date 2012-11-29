// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event.h"
#include "cc/picture_pile_impl.h"
#include "cc/rendering_stats.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {

scoped_refptr<PicturePileImpl> PicturePileImpl::Create() {
  return make_scoped_refptr(new PicturePileImpl());
}

PicturePileImpl::PicturePileImpl() {
}

PicturePileImpl::~PicturePileImpl() {
}

scoped_refptr<PicturePileImpl> PicturePileImpl::CloneForDrawing() const {
  TRACE_EVENT0("cc", "PicturePileImpl::CloneForDrawing");
  scoped_refptr<PicturePileImpl> clone = Create();
  clone->pile_.resize(pile_.size());
  for (size_t i = 0; i < pile_.size(); ++i)
    clone->pile_[i] = pile_[i]->Clone();

  return clone;
}

void PicturePileImpl::Raster(SkCanvas* canvas, gfx::Rect rect,
                             RenderingStats* stats) {
  base::TimeTicks rasterizeBeginTime = base::TimeTicks::Now();

  // TODO(enne): do this more efficiently, i.e. top down with Skia clips
  canvas->save();
  canvas->translate(-rect.x(), -rect.y());
  SkRect layer_skrect = SkRect::MakeXYWH(rect.x(), rect.y(),
                                         rect.width(), rect.height());
  canvas->clipRect(layer_skrect);
  for (size_t i = 0; i < pile_.size(); ++i) {
    if (!pile_[i]->LayerRect().Intersects(rect))
      continue;
    pile_[i]->Raster(canvas);

    SkISize deviceSize = canvas->getDeviceSize();
    stats->totalPixelsRasterized += deviceSize.width() * deviceSize.height();
  }
  canvas->restore();

  stats->totalRasterizeTimeInSeconds += (base::TimeTicks::Now() -
                                         rasterizeBeginTime).InSecondsF();
}

}  // namespace cc
