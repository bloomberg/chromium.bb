// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/picture_pile.h"

namespace cc {

PicturePile::PicturePile() {
}

PicturePile::~PicturePile() {
}

void PicturePile::invalidate(gfx::Rect rect) {
  // TODO(enne)
}

void PicturePile::resize(gfx::Size size) {
  // TODO(enne)
}

void PicturePile::update(ContentLayerClient* painter) {
  // TODO(enne)
}

void PicturePile::pushPropertiesTo(PicturePile& other) {
  other.size_ = size_;
  other.pile_.resize(pile_.size());
  for (size_t i = 0; i < pile_.size(); ++i)
    other.pile_[i] = pile_[i];
}

}  // namespace cc
