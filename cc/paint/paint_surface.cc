// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_surface.h"

namespace cc {

PaintSurface::PaintSurface(sk_sp<SkSurface> surface)
    : surface_(std::move(surface)) {}

PaintSurface::~PaintSurface() = default;

}  // namespace cc
