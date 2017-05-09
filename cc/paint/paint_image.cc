// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"

namespace cc {

PaintImage::PaintImage() = default;
PaintImage::PaintImage(sk_sp<SkImage> sk_image,
                       AnimationType animation_type,
                       CompletionState completion_state)
    : sk_image_(std::move(sk_image)),
      animation_type_(animation_type),
      completion_state_(completion_state) {}
PaintImage::PaintImage(const PaintImage& other) = default;
PaintImage::PaintImage(PaintImage&& other) = default;
PaintImage::~PaintImage() = default;

PaintImage& PaintImage::operator=(const PaintImage& other) = default;
PaintImage& PaintImage::operator=(PaintImage&& other) = default;

bool PaintImage::operator==(const PaintImage& other) {
  return sk_image_ == other.sk_image_ &&
         animation_type_ == other.animation_type_ &&
         completion_state_ == other.completion_state_;
}

}  // namespace cc
