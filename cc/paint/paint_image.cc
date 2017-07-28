// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"
#include "base/atomic_sequence_num.h"
#include "cc/paint/paint_record.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {
base::AtomicSequenceNumber s_next_id_;
}

PaintImage::PaintImage() = default;
PaintImage::PaintImage(Id id,
                       sk_sp<SkImage> sk_image,
                       AnimationType animation_type,
                       CompletionState completion_state,
                       size_t frame_count,
                       bool is_multipart)
    : sk_image_(std::move(sk_image)),
      id_(id),
      animation_type_(animation_type),
      completion_state_(completion_state),
      frame_count_(frame_count),
      is_multipart_(is_multipart) {}
PaintImage::PaintImage(const PaintImage& other) = default;
PaintImage::PaintImage(PaintImage&& other) = default;
PaintImage::~PaintImage() = default;

PaintImage& PaintImage::operator=(const PaintImage& other) = default;
PaintImage& PaintImage::operator=(PaintImage&& other) = default;

bool PaintImage::operator==(const PaintImage& other) const {
  return id_ == other.id_ && sk_image_ == other.sk_image_ &&
         animation_type_ == other.animation_type_ &&
         completion_state_ == other.completion_state_ &&
         frame_count_ == other.frame_count_ &&
         is_multipart_ == other.is_multipart_;
}

PaintImage::Id PaintImage::GetNextId() {
  return s_next_id_.GetNext();
}

PaintImage PaintImage::CloneWithSkImage(sk_sp<SkImage> new_image) const {
  PaintImage result(*this);
  result.sk_image_ = std::move(new_image);
  result.cached_sk_image_ = nullptr;
  result.paint_record_ = nullptr;
  result.paint_record_rect_ = gfx::Rect();
  return result;
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
  }
  return cached_sk_image_;
}

}  // namespace cc
