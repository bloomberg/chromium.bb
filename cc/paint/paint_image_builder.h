// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_IMAGE_BUILDER_H_
#define CC_PAINT_PAINT_IMAGE_BUILDER_H_

#include "base/memory/ptr_util.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_generator.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/skia_paint_image_generator.h"
#include "third_party/skia/include/core/SkImage.h"

namespace cc {

// Class used to construct a paint image.
class CC_PAINT_EXPORT PaintImageBuilder {
 public:
  PaintImageBuilder();
  // Starts with the given image's flags. Note that this does _not_ keep the
  // "contents" of the image. That is, it clears the cached SkImage, the set
  // SkImage, the set PaintRecord, and any other content type variables.
  explicit PaintImageBuilder(PaintImage starting_image);
  ~PaintImageBuilder();

  PaintImageBuilder& set_id(PaintImage::Id id) {
    paint_image_.id_ = id;
#if DCHECK_IS_ON()
    id_set_ = true;
#endif
    return *this;
  }
  PaintImageBuilder& set_image(sk_sp<SkImage> sk_image) {
    paint_image_.sk_image_ = std::move(sk_image);
    return *this;
  }
  PaintImageBuilder& set_paint_record(sk_sp<PaintRecord> paint_record,
                                      const gfx::Rect& rect) {
    paint_image_.paint_record_ = std::move(paint_record);
    paint_image_.paint_record_rect_ = rect;
    return *this;
  }
  PaintImageBuilder& set_animation_type(PaintImage::AnimationType type) {
    paint_image_.animation_type_ = type;
    return *this;
  }
  PaintImageBuilder& set_completion_state(PaintImage::CompletionState state) {
    paint_image_.completion_state_ = state;
    return *this;
  }
  PaintImageBuilder& set_frame_count(size_t frame_count) {
    paint_image_.frame_count_ = frame_count;
    return *this;
  }
  PaintImageBuilder& set_is_multipart(bool is_multipart) {
    paint_image_.is_multipart_ = is_multipart;
    return *this;
  }

  // |unique_id| is the id used when constructing an SkImage representation for
  // a generator backed image. |allocated_unique_id| will be set to the id
  // allocated to the resulting image.
  // TODO(khushalsagar): Remove the use of this uniqueID. See crbug.com/753639.
  PaintImageBuilder& set_paint_image_generator(
      sk_sp<PaintImageGenerator> generator,
      uint32_t unique_id = SkiaPaintImageGenerator::kNeedNewImageUniqueID,
      uint32_t* allocated_unique_id = nullptr) {
    paint_image_.paint_image_generator_ = std::move(generator);

    paint_image_.cached_sk_image_ =
        SkImage::MakeFromGenerator(std::make_unique<SkiaPaintImageGenerator>(
            paint_image_.paint_image_generator_, unique_id));
    if (allocated_unique_id)
      *allocated_unique_id = paint_image_.cached_sk_image_->uniqueID();
    return *this;
  }

  PaintImage TakePaintImage() const;

 private:
  PaintImage paint_image_;
#if DCHECK_IS_ON()
  bool id_set_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(PaintImageBuilder);
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_IMAGE_BUILDER_H_
