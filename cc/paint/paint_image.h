// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_H_
#define CC_PAINT_PAINT_IMAGE_H_

#include "base/logging.h"
#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class PaintOpBuffer;
using PaintRecord = PaintOpBuffer;

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
  // TODO(khushalsagar): The only user of these seems to be the ImageBuffer in
  // blink. Same goes for the animation and completion states.
  static const Id kUnknownStableId = -2;

  // TODO(vmpstr): Work towards removing "UNKNOWN" value.
  enum class AnimationType { UNKNOWN, ANIMATED, VIDEO, STATIC };

  // TODO(vmpstr): Work towards removing "UNKNOWN" value.
  enum class CompletionState { UNKNOWN, DONE, PARTIALLY_DONE };

  static Id GetNextId();

  PaintImage();
  // - id: stable id for this image; can be generated using GetNextId().
  // - sk_image: the underlying skia image that this represents.
  // - animation_type: the animation type of this paint image.
  // - completion_state: indicates whether the image is completed loading.
  // - frame_count: the known number of frames in this image. E.g. number of GIF
  //   frames in an animated GIF.
  explicit PaintImage(Id id,
                      sk_sp<SkImage> sk_image,
                      AnimationType animation_type = AnimationType::STATIC,
                      CompletionState completion_state = CompletionState::DONE,
                      size_t frame_count = 0,
                      bool is_multipart = false);
  PaintImage(const PaintImage& other);
  PaintImage(PaintImage&& other);
  ~PaintImage();

  PaintImage& operator=(const PaintImage& other);
  PaintImage& operator=(PaintImage&& other);

  bool operator==(const PaintImage& other) const;

  Id stable_id() const { return id_; }
  const sk_sp<SkImage>& GetSkImage() const;
  AnimationType animation_type() const { return animation_type_; }
  CompletionState completion_state() const { return completion_state_; }
  size_t frame_count() const { return frame_count_; }
  bool is_multipart() const { return is_multipart_; }

  // TODO(vmpstr): Don't get the SkImage here if you don't need to.
  explicit operator bool() const { return !!GetSkImage(); }
  bool IsLazyGenerated() const { return GetSkImage()->isLazyGenerated(); }
  int width() const { return GetSkImage()->width(); }
  int height() const { return GetSkImage()->height(); }

  // Returns a PaintImage that has the same fields as this PaintImage, except
  // with a replaced sk_image_. This can be used to swap out a specific SkImage
  // in an otherwise unchanged PaintImage. This is also used to swap out a
  // paint record representation with an SkImage instead.
  PaintImage CloneWithSkImage(sk_sp<SkImage> new_image) const;

 private:
  friend class PaintImageBuilder;

  sk_sp<SkImage> sk_image_;
  sk_sp<PaintRecord> paint_record_;
  gfx::Rect paint_record_rect_;

  Id id_ = kUnknownStableId;
  AnimationType animation_type_ = AnimationType::UNKNOWN;
  CompletionState completion_state_ = CompletionState::UNKNOWN;

  // The number of frames known to exist in this image (eg number of GIF frames
  // loaded). 0 indicates either unknown or only a single frame, both of which
  // should be treated similarly.
  size_t frame_count_ = 0;

  // Whether the data fetched for this image is a part of a multpart response.
  bool is_multipart_ = false;

  mutable sk_sp<SkImage> cached_sk_image_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_H_
