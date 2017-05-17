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
  using Id = int;

  // An id that can be used for all non-lazy images. Note that if an image is
  // not lazy, it does not mean that this id must be used; one can still use
  // GetNextId to generate a stable id for such images.
  static const Id kNonLazyStableId = -1;

  // This is the id used in places where we are currently not plumbing the
  // correct image id from blink.
  // TODO(khushalsagar): Eliminate these cases. See crbug.com/722559.
  static const Id kUnknownStableId = -2;

  // TODO(vmpstr): Work towards removing "UNKNOWN" value.
  enum class AnimationType { UNKNOWN, ANIMATED, VIDEO, STATIC };

  // TODO(vmpstr): Work towards removing "UNKNOWN" value.
  enum class CompletionState { UNKNOWN, DONE, PARTIALLY_DONE };

  static Id GetNextId();

  PaintImage();
  explicit PaintImage(Id id,
                      sk_sp<SkImage> sk_image,
                      AnimationType animation_type = AnimationType::STATIC,
                      CompletionState completion_state = CompletionState::DONE);
  PaintImage(const PaintImage& other);
  PaintImage(PaintImage&& other);
  ~PaintImage();

  PaintImage& operator=(const PaintImage& other);
  PaintImage& operator=(PaintImage&& other);

  bool operator==(const PaintImage& other) const;
  explicit operator bool() const { return sk_image_; }

  Id stable_id() const { return id_; }
  const sk_sp<SkImage>& sk_image() const { return sk_image_; }
  AnimationType animation_type() const { return animation_type_; }
  CompletionState completion_state() const { return completion_state_; }

 private:
  Id id_ = kUnknownStableId;
  sk_sp<SkImage> sk_image_;
  AnimationType animation_type_ = AnimationType::UNKNOWN;
  CompletionState completion_state_ = CompletionState::UNKNOWN;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_H_
