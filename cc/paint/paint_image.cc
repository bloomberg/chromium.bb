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

const int PaintImage::kNonLazyStableId = -1;
const size_t PaintImage::kDefaultFrameIndex = 0u;

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
         is_multipart_ == other.is_multipart_;
}

PaintImage::Id PaintImage::GetNextId() {
  return s_next_id_.GetNext();
}

PaintImage::ContentId PaintImage::GetNextContentId() {
  return s_next_content_id_.GetNext();
}

const sk_sp<SkImage>& PaintImage::GetSkImage() const {
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

  // Creating the |cached_sk_image_| using the SkImage from the original
  // PaintImage is an optimization to allow re-use of the original decode for
  // image subsets in skia, for cases that rely on skia's image decode cache.
  result.cached_sk_image_ =
      GetSkImage()->makeSubset(gfx::RectToSkIRect(subset));
  return result;
}

void PaintImage::CreateSkImage() {
  DCHECK(!cached_sk_image_);

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
            paint_image_generator_, frame_index_));
  }

  if (!subset_rect_.IsEmpty() && cached_sk_image_) {
    cached_sk_image_ =
        cached_sk_image_->makeSubset(gfx::RectToSkIRect(subset_rect_));
  }
}

PaintImage PaintImage::MakeStatic() const {
  PaintImage result(*this);
  result.repetition_count_ = kAnimationNone;
  return result;
}

SkISize PaintImage::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  // TODO(vmpstr): If this image is using subset_rect, then we don't support
  // decoding to any scale other than the original. See the comment in Decode()
  // explaining this in more detail.
  // TODO(vmpstr): For now, always decode to the original size. This can be
  // enabled with the following code, and should be done as a follow-up.
  //  if (paint_image_generator_ && subset_rect_.IsEmpty())
  //    return paint_image_generator_->GetSupportedDecodeSize(requested_size);
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
                        sk_sp<SkColorSpace> color_space,
                        size_t frame_index) const {
  // We only support decode to supported decode size.
  DCHECK(info->dimensions() == GetSupportedDecodeSize(info->dimensions()));

  // TODO(vmpstr): If we're using a subset_rect_ then the info specifies the
  // requested size relative to the subset. However, the generator isn't aware
  // of this subsetting and would need a size that is relative to the original
  // image size. We could still implement this case, but we need to convert the
  // requested size into the space of the original image. For now, fallback to
  // DecodeFromSkImage().
  if (paint_image_generator_ && subset_rect_.IsEmpty())
    return DecodeFromGenerator(memory, info, std::move(color_space),
                               frame_index);
  return DecodeFromSkImage(memory, info, std::move(color_space), frame_index);
}

bool PaintImage::DecodeFromGenerator(void* memory,
                                     SkImageInfo* info,
                                     sk_sp<SkColorSpace> color_space,
                                     size_t frame_index) const {
  DCHECK(subset_rect_.IsEmpty());

  // First convert the info to have the requested color space, since the decoder
  // will convert this for us.
  *info = info->makeColorSpace(std::move(color_space));
  if (info->colorType() != kN32_SkColorType) {
    // Since the decoders only support N32 color types, make one of those and
    // decode into temporary memory. Then read the bitmap which will convert it
    // to the target color type.
    SkImageInfo n32info = info->makeColorType(kN32_SkColorType);
    std::unique_ptr<char[]> n32memory(
        new char[n32info.minRowBytes() * n32info.height()]);

    bool result = paint_image_generator_->GetPixels(n32info, n32memory.get(),
                                                    n32info.minRowBytes(),
                                                    frame_index, unique_id());
    if (!result)
      return false;

    // The following block will use Skia to do the color type conversion from
    // N32 to the destination color type. Since color space conversion was
    // already done in GetPixels() above, remove the color space information
    // first in case Skia tries to use it for something. In practice, n32info
    // and *info color spaces match, so it should work without removing the
    // color spaces, but better be safe.
    SkImageInfo n32info_no_colorspace = n32info.makeColorSpace(nullptr);
    SkImageInfo info_no_colorspace = info->makeColorSpace(nullptr);

    SkBitmap bitmap;
    bitmap.installPixels(n32info_no_colorspace, n32memory.get(),
                         n32info.minRowBytes());
    return bitmap.readPixels(info_no_colorspace, memory, info->minRowBytes(), 0,
                             0);
  }

  return paint_image_generator_->GetPixels(*info, memory, info->minRowBytes(),
                                           frame_index, unique_id());
}

bool PaintImage::DecodeFromSkImage(void* memory,
                                   SkImageInfo* info,
                                   sk_sp<SkColorSpace> color_space,
                                   size_t frame_index) const {
  auto image = GetSkImageForFrame(frame_index);
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
  *info = info->makeColorSpace(std::move(color_space));
  return result;
}

bool PaintImage::ShouldAnimate() const {
  return animation_type_ == AnimationType::ANIMATED &&
         repetition_count_ != kAnimationNone && FrameCount() > 1;
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
  return FrameKey(content_id, frame_index, subset_rect_);
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

sk_sp<SkImage> PaintImage::GetSkImageForFrame(size_t index) const {
  DCHECK_LT(index, FrameCount());

  if (index == frame_index_)
    return GetSkImage();

  sk_sp<SkImage> image = SkImage::MakeFromGenerator(
      base::MakeUnique<SkiaPaintImageGenerator>(paint_image_generator_, index));
  if (!subset_rect_.IsEmpty())
    image = image->makeSubset(gfx::RectToSkIRect(subset_rect_));
  return image;
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
      << " is_multipart_: " << is_multipart_;
  return str.str();
}

PaintImage::FrameKey::FrameKey(ContentId content_id,
                               size_t frame_index,
                               gfx::Rect subset_rect)
    : content_id_(content_id),
      frame_index_(frame_index),
      subset_rect_(subset_rect) {
  size_t original_hash = base::HashInts(static_cast<uint64_t>(content_id_),
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
  return content_id_ == other.content_id_ &&
         frame_index_ == other.frame_index_ &&
         subset_rect_ == other.subset_rect_;
}

bool PaintImage::FrameKey::operator!=(const FrameKey& other) const {
  return !(*this == other);
}

std::string PaintImage::FrameKey::ToString() const {
  std::ostringstream str;
  str << "content_id: " << content_id_ << ","
      << "frame_index: " << frame_index_ << ","
      << "subset_rect: " << subset_rect_.ToString();
  return str.str();
}

}  // namespace cc
