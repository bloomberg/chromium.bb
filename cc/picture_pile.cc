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

void PicturePile::Invalidate(gfx::Rect rect) {
  invalidation_.Union(rect);
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
  if (size.width() > size_.width()) {
    gfx::Rect invalid(
        size_.width(),
        0,
        size.width() - size_.width(),
        size.height());
    Invalidate(invalid);
  }
  if (size.height() > size_.height()) {
    gfx::Rect invalid(
        0,
        size_.height(),
        size.width(),
        size.height() - size_.height());
    Invalidate(invalid);
  }

  // Remove pictures that aren't in bounds anymore.
  if (size.width() < size_.width() || size.height() < size_.height()) {
    OutOfBoundsPredicate oob(size);
    pile_.erase(std::remove_if(pile_.begin(), pile_.end(), oob), pile_.end());
  }

  size_ = size;
}

void PicturePile::Update(ContentLayerClient* painter, RenderingStats& stats) {
  // WebKit paints (i.e. recording) can cause invalidations, so record previous.
  invalidation_.Swap(prev_invalidation_);
  invalidation_.Clear();

  // TODO(enne): Add things to the pile, consolidate if needed, etc...
  if (pile_.size() == 0)
    pile_.push_back(Picture::Create());
  pile_[0]->Record(painter, gfx::Rect(gfx::Point(), size_), stats);
}

void PicturePile::CopyAllButPile(
    const PicturePile& from, PicturePile& to) const {
  to.size_ = from.size_;
  to.invalidation_ = from.invalidation_;
  to.prev_invalidation_ = from.prev_invalidation_;
}

void PicturePile::PushPropertiesTo(PicturePile& other) {
  CopyAllButPile(*this, other);

  other.pile_.resize(pile_.size());
  for (size_t i = 0; i < pile_.size(); ++i)
    other.pile_[i] = pile_[i];
}

scoped_ptr<PicturePile> PicturePile::CloneForDrawing() const {
  TRACE_EVENT0("cc", "PicturePile::CloneForDrawing");
  scoped_ptr<PicturePile> clone = make_scoped_ptr(new PicturePile);
  CopyAllButPile(*this, *clone);

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
