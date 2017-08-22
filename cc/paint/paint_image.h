// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_H_
#define CC_PAINT_PAINT_IMAGE_H_

#include "base/logging.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/skia_paint_image_generator.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class PaintImageGenerator;
class PaintOpBuffer;
using PaintRecord = PaintOpBuffer;

// A representation of an image for the compositor.
// Note that aside from default construction, it can only be constructed using a
// PaintImageBuilder, or copied/moved into using operator=.
class CC_PAINT_EXPORT PaintImage {
 public:
  using Id = int;

  // An id that can be used for all non-lazy images. Note that if an image is
  // not lazy, it does not mean that this id must be used; one can still use
  // GetNextId to generate a stable id for such images.
  static const Id kNonLazyStableId = -1;

  enum class AnimationType { ANIMATED, VIDEO, STATIC };
  enum class CompletionState { DONE, PARTIALLY_DONE };

  static Id GetNextId();

  PaintImage();
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
  uint32_t unique_id() const { return GetSkImage()->uniqueID(); }
  explicit operator bool() const { return !!GetSkImage(); }
  bool IsLazyGenerated() const { return GetSkImage()->isLazyGenerated(); }
  int width() const { return GetSkImage()->width(); }
  int height() const { return GetSkImage()->height(); }

 private:
  friend class PaintImageBuilder;

  sk_sp<SkImage> sk_image_;
  sk_sp<PaintRecord> paint_record_;
  gfx::Rect paint_record_rect_;
  sk_sp<PaintImageGenerator> paint_image_generator_;

  Id id_ = 0;
  AnimationType animation_type_ = AnimationType::STATIC;
  CompletionState completion_state_ = CompletionState::DONE;

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
