// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"
#include "base/atomic_sequence_num.h"

namespace cc {
namespace {
base::StaticAtomicSequenceNumber s_next_id_;
}

PaintImage::PaintImage() = default;
PaintImage::PaintImage(Id id,
                       sk_sp<SkImage> sk_image,
                       AnimationType animation_type,
                       CompletionState completion_state,
                       size_t frame_count,
                       bool is_multipart)
    : id_(id),
      sk_image_(std::move(sk_image)),
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

}  // namespace cc
