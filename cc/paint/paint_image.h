// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_H_
#define CC_PAINT_PAINT_IMAGE_H_

#include "cc/paint/paint_export.h"

#include "third_party/skia/include/core/SkImage.h"

namespace cc {

// TODO(vmpstr): Add a persistent id to the paint image.
class CC_PAINT_EXPORT PaintImage {
 public:
  // TODO(vmpstr): Work towards removing "UNKNOWN" value.
  enum class AnimationType { UNKNOWN, ANIMATED, VIDEO, STATIC };

  // TODO(vmpstr): Work towards removing "UNKNOWN" value.
  enum class CompletionState { UNKNOWN, DONE, PARTIALLY_DONE };

  PaintImage(sk_sp<const SkImage> sk_image,
             AnimationType animation_type,
             CompletionState completion_state);
  PaintImage(const PaintImage& image);
  ~PaintImage();

  const sk_sp<const SkImage>& sk_image() const { return sk_image_; }
  AnimationType animation_type() const { return animation_type_; }
  CompletionState completion_state() const { return completion_state_; }

 private:
  sk_sp<const SkImage> sk_image_;
  AnimationType animation_type_;
  CompletionState completion_state_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_H_
