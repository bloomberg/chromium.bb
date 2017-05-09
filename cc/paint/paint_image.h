// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_H_
#define CC_PAINT_PAINT_IMAGE_H_

#include "base/logging.h"
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

  PaintImage();
  explicit PaintImage(sk_sp<SkImage> sk_image,
                      AnimationType animation_type = AnimationType::STATIC,
                      CompletionState completion_state = CompletionState::DONE);
  PaintImage(const PaintImage& other);
  PaintImage(PaintImage&& other);
  ~PaintImage();

  PaintImage& operator=(const PaintImage& other);
  PaintImage& operator=(PaintImage&& other);

  bool operator==(const PaintImage& other);
  explicit operator bool() const { return sk_image_; }

  const sk_sp<SkImage>& sk_image() const { return sk_image_; }
  AnimationType animation_type() const { return animation_type_; }
  CompletionState completion_state() const { return completion_state_; }

 private:
  sk_sp<SkImage> sk_image_;
  AnimationType animation_type_ = AnimationType::UNKNOWN;
  CompletionState completion_state_ = CompletionState::UNKNOWN;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_H_
