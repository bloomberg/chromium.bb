// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"

namespace cc {

PaintImage::PaintImage(sk_sp<const SkImage> sk_image,
                       AnimationType animation_type,
                       CompletionState completion_state)
    : sk_image_(std::move(sk_image)),
      animation_type_(animation_type),
      completion_state_(completion_state) {}

PaintImage::PaintImage(const PaintImage& image) = default;

PaintImage::~PaintImage() = default;

}  // namespace cc
