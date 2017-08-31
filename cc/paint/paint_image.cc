// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"

#include "base/atomic_sequence_num.h"
#include "base/hash.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/paint_image_generator.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/skia_paint_image_generator.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {
base::AtomicSequenceNumber s_next_id_;
base::AtomicSequenceNumber s_next_content_id_;
}  // namespace

PaintImage::PaintImage() = default;
PaintImage::PaintImage(const PaintImage& other) = default;
PaintImage::PaintImage(PaintImage&& other) = default;
PaintImage::~PaintImage() = default;

PaintImage& PaintImage::operator=(const PaintImage& other) = default;
PaintImage& PaintImage::operator=(PaintImage&& other) = default;

bool PaintImage::operator==(const PaintImage& other) const {
  return sk_image_ == other.sk_image_ && paint_record_ == other.paint_record_ &&
         paint_record_rect_ == other.paint_record_rect_ &&
         paint_record_content_id_ == other.paint_record_content_id_ &&
         paint_image_generator_ == other.paint_image_generator_ &&
         id_ == other.id_ && animation_type_ == other.animation_type_ &&
         completion_state_ == other.completion_state_ &&
         subset_rect_ == other.subset_rect_ &&
         frame_index_ == other.frame_index_ &&
         is_multipart_ == other.is_multipart_ &&
         sk_image_id_ == other.sk_image_id_;
}

PaintImage::Id PaintImage::GetNextId() {
  return s_next_id_.GetNext();
}

PaintImage::ContentId PaintImage::GetNextContentId() {
  return s_next_content_id_.GetNext();
}

const sk_sp<SkImage>& PaintImage::GetSkImage() const {
  if (cached_sk_image_)
    return cached_sk_image_;

  if (sk_image_) {
    cached_sk_image_ = sk_image_;
  } else if (paint_record_) {
    cached_sk_image_ = SkImage::MakeFromPicture(
        ToSkPicture(paint_record_, gfx::RectToSkRect(paint_record_rect_)),
        SkISize::Make(paint_record_rect_.width(), paint_record_rect_.height()),
        nullptr, nullptr, SkImage::BitDepth::kU8, SkColorSpace::MakeSRGB());
  } else if (paint_image_generator_) {
    cached_sk_image_ =
        SkImage::MakeFromGenerator(base::MakeUnique<SkiaPaintImageGenerator>(
            paint_image_generator_, frame_index_, sk_image_id_));
  }

  if (!subset_rect_.IsEmpty() && cached_sk_image_) {
    cached_sk_image_ =
        cached_sk_image_->makeSubset(gfx::RectToSkIRect(subset_rect_));
  }
  return cached_sk_image_;
}

PaintImage PaintImage::MakeSubset(const gfx::Rect& subset) const {
  DCHECK(!subset.IsEmpty());

  // If the subset is the same as the image bounds, we can return the same
  // image.
  gfx::Rect bounds(width(), height());
  if (bounds == subset)
    return *this;

  DCHECK(bounds.Contains(subset))
      << "Subset should not be greater than the image bounds";
  PaintImage result(*this);
  result.subset_rect_ = subset;
  // Store the subset from the original image.
  result.subset_rect_.Offset(subset_rect_.x(), subset_rect_.y());

  // Creating the |cached_sk_image_| is an optimization to allow re-use of the
  // original decode for image subsets in skia, for cases that rely on skia's
  // image decode cache.
  // TODO(khushalsagar): Remove this when we no longer have such cases. See
  // crbug.com/753639.
  result.cached_sk_image_ =
      GetSkImage()->makeSubset(gfx::RectToSkIRect(subset));
  return result;
}

SkISize PaintImage::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  // TODO(vmpstr): For now, we ignore the requested size and just return the
  // available image size.
  return SkISize::Make(width(), height());
}

SkImageInfo PaintImage::CreateDecodeImageInfo(const SkISize& size,
                                              SkColorType color_type) const {
  DCHECK(GetSupportedDecodeSize(size) == size);
  return SkImageInfo::Make(size.width(), size.height(), color_type,
                           kPremul_SkAlphaType);
}

bool PaintImage::Decode(void* memory,
                        SkImageInfo* info,
                        sk_sp<SkColorSpace> color_space) const {
  auto image = GetSkImage();
  DCHECK(image);
  if (color_space) {
    image =
        image->makeColorSpace(color_space, SkTransferFunctionBehavior::kIgnore);
    if (!image)
      return false;
  }
  // Note that the readPixels has to happen before converting the info to the
  // given color space, since it can produce incorrect results.
  bool result = image->readPixels(*info, memory, info->minRowBytes(), 0, 0,
                                  SkImage::kDisallow_CachingHint);
  *info = info->makeColorSpace(color_space);
  return result;
}

PaintImage::FrameKey PaintImage::GetKeyForFrame(size_t frame_index) const {
  DCHECK_LT(frame_index, FrameCount());
  DCHECK(paint_image_generator_ || paint_record_);

  // Query the content id that uniquely identifies the content for this frame
  // from the content provider.
  ContentId content_id = kInvalidContentId;
  if (paint_image_generator_)
    content_id = paint_image_generator_->GetContentIdForFrame(frame_index);
  else
    content_id = paint_record_content_id_;

  DCHECK_NE(content_id, kInvalidContentId);
  return FrameKey(id_, content_id, frame_index, subset_rect_);
}

const std::vector<FrameMetadata>& PaintImage::GetFrameMetadata() const {
  DCHECK_EQ(animation_type_, AnimationType::ANIMATED);
  DCHECK(paint_image_generator_);

  return paint_image_generator_->GetFrameMetadata();
}

size_t PaintImage::FrameCount() const {
  if (!GetSkImage())
    return 0u;
  return paint_image_generator_
             ? paint_image_generator_->GetFrameMetadata().size()
             : 1u;
}

std::string PaintImage::ToString() const {
  std::ostringstream str;
  str << "sk_image_: " << sk_image_ << " paint_record_: " << paint_record_
      << " paint_record_rect_: " << paint_record_rect_.ToString()
      << " paint_image_generator_: " << paint_image_generator_
      << " id_: " << id_
      << " animation_type_: " << static_cast<int>(animation_type_)
      << " completion_state_: " << static_cast<int>(completion_state_)
      << " subset_rect_: " << subset_rect_.ToString()
      << " frame_index_: " << frame_index_
      << " is_multipart_: " << is_multipart_
      << " sk_image_id_: " << sk_image_id_;
  return str.str();
}

PaintImage::FrameKey::FrameKey(Id paint_image_id,
                               ContentId content_id,
                               size_t frame_index,
                               gfx::Rect subset_rect)
    : paint_image_id_(paint_image_id),
      content_id_(content_id),
      frame_index_(frame_index),
      subset_rect_(subset_rect) {
  size_t original_hash = base::HashInts(
      static_cast<uint64_t>(base::HashInts(paint_image_id_, content_id_)),
      static_cast<uint64_t>(frame_index_));
  if (subset_rect_.IsEmpty()) {
    hash_ = original_hash;
  } else {
    size_t subset_hash =
        base::HashInts(static_cast<uint64_t>(
                           base::HashInts(subset_rect_.x(), subset_rect_.y())),
                       static_cast<uint64_t>(base::HashInts(
                           subset_rect_.width(), subset_rect_.height())));
    hash_ = base::HashInts(original_hash, subset_hash);
  }
}

bool PaintImage::FrameKey::operator==(const FrameKey& other) const {
  return paint_image_id_ == other.paint_image_id_ &&
         content_id_ == other.content_id_ &&
         frame_index_ == other.frame_index_ &&
         subset_rect_ == other.subset_rect_;
}

bool PaintImage::FrameKey::operator!=(const FrameKey& other) const {
  return !(*this == other);
}

std::string PaintImage::FrameKey::ToString() const {
  std::ostringstream str;
  str << "paint_image_id: " << paint_image_id_ << ","
      << "content_id: " << content_id_ << ","
      << "frame_index: " << frame_index_ << ","
      << "subset_rect: " << subset_rect_.ToString();
  return str.str();
}

}  // namespace cc
