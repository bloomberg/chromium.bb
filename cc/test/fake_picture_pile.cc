// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_picture_pile.h"

#include "cc/test/fake_picture_pile_impl.h"

namespace cc {

scoped_refptr<RasterSource> FakePicturePile::CreateRasterSource() const {
  return FakePicturePileImpl::CreateFromPile(this, playback_allowed_event_);
}

}  // namespace cc
