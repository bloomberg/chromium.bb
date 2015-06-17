// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/capture_resolution_chooser.h"

#include "media/base/limits.h"
#include "media/base/video_util.h"

namespace media {

namespace {

// Compute the minimum frame size from the given |max_frame_size| and
// |resolution_change_policy|.
gfx::Size ComputeMinimumCaptureSize(
    const gfx::Size& max_frame_size,
    ResolutionChangePolicy resolution_change_policy) {
  switch (resolution_change_policy) {
    case RESOLUTION_POLICY_FIXED_RESOLUTION:
      return max_frame_size;
    case RESOLUTION_POLICY_FIXED_ASPECT_RATIO: {
      // TODO(miu): This is a place-holder until "min constraints" are plumbed-
      // in from the MediaStream framework.  http://crbug.com/473336
      const int kMinLines = 180;
      if (max_frame_size.height() <= kMinLines)
        return max_frame_size;
      const gfx::Size result(
          kMinLines * max_frame_size.width() / max_frame_size.height(),
          kMinLines);
      if (result.width() <= 0 || result.width() > limits::kMaxDimension)
        return max_frame_size;
      return result;
    }
    case RESOLUTION_POLICY_ANY_WITHIN_LIMIT:
      return gfx::Size(1, 1);
    case RESOLUTION_POLICY_LAST:
      break;
  }
  NOTREACHED();
  return gfx::Size(1, 1);
}

// Returns |size|, unless it exceeds |max_size| or is under |min_size|.  When
// the bounds are exceeded, computes and returns an alternate size of similar
// aspect ratio that is within the bounds.
gfx::Size ComputeBoundedCaptureSize(const gfx::Size& size,
                                    const gfx::Size& min_size,
                                    const gfx::Size& max_size) {
  if (size.width() > max_size.width() || size.height() > max_size.height()) {
    gfx::Size result = ScaleSizeToFitWithinTarget(size, max_size);
    result.SetToMax(min_size);
    return result;
  } else if (size.width() < min_size.width() ||
             size.height() < min_size.height()) {
    gfx::Size result = ScaleSizeToEncompassTarget(size, min_size);
    result.SetToMin(max_size);
    return result;
  } else {
    return size;
  }
}

}  // namespace

CaptureResolutionChooser::CaptureResolutionChooser(
    const gfx::Size& max_frame_size,
    ResolutionChangePolicy resolution_change_policy)
    : max_frame_size_(max_frame_size),
      min_frame_size_(ComputeMinimumCaptureSize(max_frame_size,
                                                resolution_change_policy)),
      resolution_change_policy_(resolution_change_policy),
      constrained_size_(max_frame_size) {
  DCHECK_LT(0, max_frame_size_.width());
  DCHECK_LT(0, max_frame_size_.height());
  DCHECK_LE(min_frame_size_.width(), max_frame_size_.width());
  DCHECK_LE(min_frame_size_.height(), max_frame_size_.height());

  RecomputeCaptureSize();
}

CaptureResolutionChooser::~CaptureResolutionChooser() {}

void CaptureResolutionChooser::SetSourceSize(const gfx::Size& source_size) {
  if (source_size.IsEmpty())
    return;

  switch (resolution_change_policy_) {
    case RESOLUTION_POLICY_FIXED_RESOLUTION:
      // Source size changes do not affect the frame resolution.  Frame
      // resolution is always fixed to |max_frame_size_|.
      break;

    case RESOLUTION_POLICY_FIXED_ASPECT_RATIO:
      constrained_size_ = ComputeBoundedCaptureSize(
          PadToMatchAspectRatio(source_size, max_frame_size_),
          min_frame_size_,
          max_frame_size_);
      RecomputeCaptureSize();
      break;

    case RESOLUTION_POLICY_ANY_WITHIN_LIMIT:
      constrained_size_ = ComputeBoundedCaptureSize(
          source_size, min_frame_size_, max_frame_size_);
      RecomputeCaptureSize();
      break;

    case RESOLUTION_POLICY_LAST:
      NOTREACHED();
  }
}

void CaptureResolutionChooser::RecomputeCaptureSize() {
  // TODO(miu): An upcoming change will introduce the ability to find the best
  // capture resolution, given the current capabilities of the system.
  // http://crbug.com/156767
  capture_size_ = constrained_size_;
}

}  // namespace media
