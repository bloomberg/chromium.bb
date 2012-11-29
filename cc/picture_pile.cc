// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cc/picture_pile.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

PicturePile::PicturePile() {
}

PicturePile::~PicturePile() {
}

class OutOfBoundsPredicate {
public:
  OutOfBoundsPredicate(gfx::Size size) : layer_rect_(gfx::Point(), size) { }
  bool operator()(const scoped_refptr<Picture>& picture) {
    return !picture->LayerRect().Intersects(layer_rect_);
  }
  gfx::Rect layer_rect_;
};

void PicturePile::Resize(gfx::Size size) {
  // Remove pictures that aren't in bounds anymore.
  if (size.width() < size_.width() || size.height() < size_.height()) {
    OutOfBoundsPredicate oob(size);
    pile_.erase(std::remove_if(pile_.begin(), pile_.end(), oob), pile_.end());
  }

  size_ = size;
}

void PicturePile::Update(
    ContentLayerClient* painter,
    const Region&,
    RenderingStats& stats) {
  // TODO(enne): Add things to the pile, consolidate if needed, etc...
  // TODO(enne): Only re-record invalidated areas.
  // TODO(enne): Also re-record areas that have been newly exposed by resize.

  // Always re-record the entire layer into a single picture, just to get
  // this class up and running.
  pile_.clear();
  pile_.push_back(Picture::Create());
  pile_[0]->Record(painter, gfx::Rect(gfx::Point(), size_), stats);
}

void PicturePile::PushPropertiesTo(PicturePile& other) {
  other.pile_.resize(pile_.size());
  for (size_t i = 0; i < pile_.size(); ++i)
    other.pile_[i] = pile_[i];
}

scoped_ptr<PicturePile> PicturePile::CloneForDrawing() const {
  TRACE_EVENT0("cc", "PicturePile::CloneForDrawing");
  scoped_ptr<PicturePile> clone = make_scoped_ptr(new PicturePile);
  clone->pile_.resize(pile_.size());
  for (size_t i = 0; i < pile_.size(); ++i)
    clone->pile_[i] = pile_[i]->Clone();

  return clone.Pass();
}

void PicturePile::Raster(SkCanvas* canvas, gfx::Rect rect) {
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
  }
  canvas->restore();
}

}  // namespace cc
